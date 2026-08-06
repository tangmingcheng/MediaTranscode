#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"

#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status observeUniqueParameterSet(
    std::optional<std::vector<std::uint8_t>>& destination,
    std::span<const std::uint8_t> bytes,
    const char* owner)
{
    if (bytes.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " parameter set is empty"));
    }
    if (!destination) {
        destination.emplace(bytes.begin(), bytes.end());
        return ::media::Status::success();
    }
    if (std::equal(destination->begin(), destination->end(),
                   bytes.begin(), bytes.end())) {
        return ::media::Status::success();
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            std::string(owner) + " probe observed conflicting parameter sets"));
}

std::string h264ProfileLevelId(std::span<const std::uint8_t> sps)
{
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t index = 1; index <= 3; ++index) {
        output << std::setw(2) << static_cast<unsigned int>(sps[index]);
    }
    return output.str();
}

::media::Result<std::vector<std::uint8_t>> requiredParameterSet(
    const MediaRtpFmtpParameters& parameters,
    const char* key)
{
    const auto found = parameters.find(key);
    if (found == parameters.end()) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("RTP video fmtp requires ") + key));
    }
    return decodeRtpFmtpBase64(found->second);
}

::media::Result<void> validateH264ParameterSet(
    std::span<const std::uint8_t> bytes,
    std::uint8_t expectedType,
    const char* owner)
{
    if (bytes.empty() || (expectedType == 7 && bytes.size() < 4) ||
        (bytes[0] & 0x1f) != expectedType) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("RTP video fmtp has malformed ") + owner));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateHevcParameterSet(
    std::span<const std::uint8_t> bytes,
    std::uint8_t expectedType,
    const char* owner)
{
    if (bytes.size() < 2 || ((bytes[0] >> 1) & 0x3f) != expectedType) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("RTP video fmtp has malformed ") + owner));
    }
    return ::media::Result<void>::success();
}

::media::Result<int> requiredPacketizationMode(
    const MediaRtpFmtpParameters& parameters)
{
    const auto found = parameters.find("packetization-mode");
    if (found == parameters.end()) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video fmtp requires packetization-mode"));
    }
    int mode = 0;
    const auto parsed = std::from_chars(
        found->second.data(), found->second.data() + found->second.size(), mode);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != found->second.data() + found->second.size() || mode != 1) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP H264 fmtp requires packetization-mode=1"));
    }
    return ::media::Result<int>::success(mode);
}

bool isHexProfileLevelId(const std::string& value)
{
    return value.size() == 6 && std::all_of(
        value.begin(), value.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        });
}

} // namespace

MediaRtpVideoSignalingObserver::MediaRtpVideoSignalingObserver(
    std::string codecName,
    std::uint8_t payloadType,
    int clockRate,
    NalParser parser)
    : m_codecName(std::move(codecName)),
      m_payloadType(payloadType),
      m_clockRate(clockRate),
      m_parser(std::move(parser))
{
}

::media::Result<MediaRtpVideoSignalingObserver>
MediaRtpVideoSignalingObserver::create(std::string codecName,
                                       std::uint8_t payloadType,
                                       int clockRate,
                                       MediaRtpVideoPacketizationPolicy packetizationPolicy)
{
    codecName = canonicalCodecName(codecName);
    if (payloadType < 96 || payloadType > 127 || clockRate != 90'000) {
        return ::media::Result<MediaRtpVideoSignalingObserver>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video signaling probe requires dynamic PT and 90000 Hz clock"));
    }
    if (codecName == "h264" && packetizationPolicy ==
            MediaRtpVideoPacketizationPolicy::H264NonInterleaved) {
        return ::media::Result<MediaRtpVideoSignalingObserver>::success(
            MediaRtpVideoSignalingObserver(
                std::move(codecName), payloadType, clockRate,
                NalParser(MediaH264RtpNalUnitParser(payloadType))));
    }
    if (codecName == "hevc" && packetizationPolicy ==
            MediaRtpVideoPacketizationPolicy::HevcNonInterleavedNoDonl) {
        return ::media::Result<MediaRtpVideoSignalingObserver>::success(
            MediaRtpVideoSignalingObserver(
                std::move(codecName), payloadType, clockRate,
                NalParser(MediaHevcRtpNalUnitParser(payloadType))));
    }
    return ::media::Result<MediaRtpVideoSignalingObserver>::failure(
        ::media::ErrorInfo::unsupported(
            "RTP video signaling codec conflicts with planned packetization policy"));
}

void MediaRtpVideoSignalingObserver::resetEpoch(std::uint32_t ssrc) noexcept
{
    std::visit([](auto& parser) { parser.discontinuity(); }, m_parser);
    m_ssrc = ssrc;
    m_vps.reset();
    m_sps.reset();
    m_pps.reset();
}

bool MediaRtpVideoSignalingObserver::complete() const noexcept
{
    return m_codecName == "h264"
        ? m_sps.has_value() && m_pps.has_value()
        : m_vps.has_value() && m_sps.has_value() && m_pps.has_value();
}

::media::Result<MediaRtpVideoSignalingObservation>
MediaRtpVideoSignalingObserver::observe(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_payloadType) {
        return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video signaling packet payload type conflicts with probe plan"));
    }
    bool epochChanged = false;
    if (!m_ssrc || *m_ssrc != packet.ssrc) {
        epochChanged = m_ssrc.has_value();
        resetEpoch(packet.ssrc);
    }

    auto parsed = std::visit(
        [&packet](auto& parser) { return parser.push(packet); }, m_parser);
    if (!parsed) {
        return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
            parsed.error());
    }
    if (parsed.value().discardedFragment) {
        return ::media::Result<MediaRtpVideoSignalingObservation>::success(
            {epochChanged, false});
    }
    for (const auto& nal : parsed.value().nalUnits) {
        const auto bytes = nal.bytes();
        if (m_codecName == "h264") {
            const std::uint8_t type = bytes[0] & 0x1f;
            if (type == 7) {
                if (bytes.size() < 4) {
                    return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "H264 SPS is too short for profile-level-id"));
                }
                if (auto status = observeUniqueParameterSet(
                        m_sps, bytes, "H264 SPS"); !status) {
                    return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
                        status.error());
                }
            } else if (type == 8) {
                if (auto status = observeUniqueParameterSet(
                        m_pps, bytes, "H264 PPS"); !status) {
                    return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
                        status.error());
                }
            }
        } else {
            if (bytes.size() < 2) {
                return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "HEVC parameter-set NAL is truncated"));
            }
            const std::uint8_t type = static_cast<std::uint8_t>(
                (bytes[0] >> 1) & 0x3f);
            std::optional<std::vector<std::uint8_t>>* destination = nullptr;
            const char* owner = nullptr;
            if (type == 32) {
                destination = &m_vps;
                owner = "HEVC VPS";
            } else if (type == 33) {
                destination = &m_sps;
                owner = "HEVC SPS";
            } else if (type == 34) {
                destination = &m_pps;
                owner = "HEVC PPS";
            }
            if (destination) {
                if (auto status = observeUniqueParameterSet(
                        *destination, bytes, owner); !status) {
                    return ::media::Result<MediaRtpVideoSignalingObservation>::failure(
                        status.error());
                }
            }
        }
    }
    return ::media::Result<MediaRtpVideoSignalingObservation>::success(
        {epochChanged, complete()});
}

void MediaRtpVideoSignalingObserver::discontinuity() noexcept
{
    std::visit([](auto& parser) { parser.discontinuity(); }, m_parser);
}

::media::Result<MediaDetectedRtpVideoSignaling>
MediaRtpVideoSignalingObserver::detected(
    std::size_t packetCount,
    std::size_t datagramBytes,
    std::int64_t elapsedMilliseconds) const
{
    if (!complete() || !m_ssrc) {
        return ::media::Result<MediaDetectedRtpVideoSignaling>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP video signaling probe has incomplete parameter-set evidence"));
    }
    MediaRtpVideoSignalingFacts facts;
    if (m_codecName == "h264") {
        facts = MediaH264SignalingFacts{
            *m_sps, *m_pps, h264ProfileLevelId(*m_sps)};
    } else {
        facts = MediaHevcSignalingFacts{*m_vps, *m_sps, *m_pps};
    }
    return ::media::Result<MediaDetectedRtpVideoSignaling>::success({
        m_codecName,
        m_payloadType,
        m_clockRate,
        *m_ssrc,
        std::move(facts),
        packetCount,
        datagramBytes,
        elapsedMilliseconds});
}

MediaRtpVideoSignalingEvidence
MediaRtpVideoSignalingObserver::evidence() const noexcept
{
    return MediaRtpVideoSignalingEvidence{
        m_ssrc, m_vps.has_value(), m_sps.has_value(), m_pps.has_value()};
}

::media::Result<std::string> serializeRtpVideoFmtp(
    const MediaRtpVideoSignalingFacts& facts)
{
    if (const auto* h264 = std::get_if<MediaH264SignalingFacts>(&facts)) {
        if (h264->profileLevelId.size() != 6) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "H264 signaling facts require a six-digit profile-level-id"));
        }
        auto sps = encodeRtpFmtpBase64(h264->sps);
        auto pps = encodeRtpFmtpBase64(h264->pps);
        if (!sps || !pps) {
            return ::media::Result<std::string>::failure(
                !sps ? sps.error() : pps.error());
        }
        return ::media::Result<std::string>::success(
            "packetization-mode=1;sprop-parameter-sets=" + sps.value() +
            "," + pps.value() + ";profile-level-id=" +
            h264->profileLevelId);
    }
    const auto& hevc = std::get<MediaHevcSignalingFacts>(facts);
    auto vps = encodeRtpFmtpBase64(hevc.vps);
    auto sps = encodeRtpFmtpBase64(hevc.sps);
    auto pps = encodeRtpFmtpBase64(hevc.pps);
    if (!vps || !sps || !pps) {
        return ::media::Result<std::string>::failure(
            !vps ? vps.error() : (!sps ? sps.error() : pps.error()));
    }
    return ::media::Result<std::string>::success(
        "sprop-vps=" + vps.value() + ";sprop-sps=" + sps.value() +
        ";sprop-pps=" + pps.value());
}

::media::Result<MediaRtpVideoSignalingFacts> parseRtpVideoSignalingFacts(
    const std::string& requestedCodecName,
    const std::string& fmtp)
{
    const std::string codecName = canonicalCodecName(requestedCodecName);
    auto parameters = parseRtpFmtp(fmtp);
    if (!parameters) {
        return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
            parameters.error());
    }

    if (codecName == "h264") {
        auto packetizationMode = requiredPacketizationMode(parameters.value());
        if (!packetizationMode) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                packetizationMode.error());
        }
        const auto parameterSets =
            parameters.value().find("sprop-parameter-sets");
        const auto profileLevelId = parameters.value().find("profile-level-id");
        if (parameterSets == parameters.value().end() ||
            profileLevelId == parameters.value().end() ||
            !isHexProfileLevelId(profileLevelId->second)) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP H264 fmtp requires a six-digit hexadecimal profile-level-id and sprop-parameter-sets"));
        }
        const auto separator = parameterSets->second.find(',');
        if (separator == std::string::npos ||
            parameterSets->second.find(',', separator + 1) != std::string::npos) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP H264 fmtp requires exactly one SPS and one PPS"));
        }
        auto sps = decodeRtpFmtpBase64(parameterSets->second.substr(0, separator));
        auto pps = decodeRtpFmtpBase64(parameterSets->second.substr(separator + 1));
        if (!sps || !pps) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                !sps ? sps.error() : pps.error());
        }
        if (auto status = validateH264ParameterSet(sps.value(), 7, "SPS");
            !status) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                status.error());
        }
        if (auto status = validateH264ParameterSet(pps.value(), 8, "PPS");
            !status) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                status.error());
        }
        const std::string derivedProfileLevelId = h264ProfileLevelId(sps.value());
        std::string normalizedProfileLevelId = profileLevelId->second;
        std::transform(normalizedProfileLevelId.begin(), normalizedProfileLevelId.end(),
                       normalizedProfileLevelId.begin(), [](unsigned char character) {
                           return static_cast<char>(std::toupper(character));
                       });
        if (normalizedProfileLevelId != derivedProfileLevelId) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP H264 fmtp profile-level-id conflicts with SPS"));
        }
        return ::media::Result<MediaRtpVideoSignalingFacts>::success(
            MediaH264SignalingFacts{
                std::move(sps).value(), std::move(pps).value(),
                std::move(derivedProfileLevelId)});
    }

    if (codecName == "hevc") {
        auto vps = requiredParameterSet(parameters.value(), "sprop-vps");
        auto sps = requiredParameterSet(parameters.value(), "sprop-sps");
        auto pps = requiredParameterSet(parameters.value(), "sprop-pps");
        if (!vps || !sps || !pps) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                !vps ? vps.error() : (!sps ? sps.error() : pps.error()));
        }
        if (auto status = validateHevcParameterSet(vps.value(), 32, "VPS");
            !status) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                status.error());
        }
        if (auto status = validateHevcParameterSet(sps.value(), 33, "SPS");
            !status) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                status.error());
        }
        if (auto status = validateHevcParameterSet(pps.value(), 34, "PPS");
            !status) {
            return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
                status.error());
        }
        return ::media::Result<MediaRtpVideoSignalingFacts>::success(
            MediaHevcSignalingFacts{
                std::move(vps).value(), std::move(sps).value(),
                std::move(pps).value()});
    }

    return ::media::Result<MediaRtpVideoSignalingFacts>::failure(
        ::media::ErrorInfo::unsupported(
            "RTP video fmtp parsing supports only H264 and HEVC"));
}

} // namespace media::ffmpeg::graph
