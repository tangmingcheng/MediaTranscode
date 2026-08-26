#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"

#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/utils/MediaAsciiStringUtils.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <array>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t RtpHeaderBytes = 12;
constexpr std::uint64_t AacAuHeadersLengthBytes = 2;
constexpr std::uint64_t AacAuHeaderBytes = 2;
constexpr std::uint64_t AacMinimumPayloadBytes = 1;
constexpr std::uint64_t AacSizeLengthBits = 13;
constexpr std::uint64_t H264AnnexBExpansionNumerator = 5;
constexpr std::uint64_t HevcAnnexBExpansionNumerator = 3;

struct H264LevelLimit final {
    std::uint8_t levelIdc;
    std::uint64_t maximumCpbKilobits;
};

// ITU-T H.264 (08/2021), Table A-1, MaxCPB column. Level 1b is
// represented separately because it shares level_idc 11.
constexpr std::array H264LevelLimits{
    H264LevelLimit{10, 175}, H264LevelLimit{11, 500},
    H264LevelLimit{12, 1'000}, H264LevelLimit{13, 2'000},
    H264LevelLimit{20, 2'000}, H264LevelLimit{21, 4'000},
    H264LevelLimit{22, 4'000}, H264LevelLimit{30, 10'000},
    H264LevelLimit{31, 14'000}, H264LevelLimit{32, 20'000},
    H264LevelLimit{40, 25'000}, H264LevelLimit{41, 62'500},
    H264LevelLimit{42, 62'500}, H264LevelLimit{50, 135'000},
    H264LevelLimit{51, 240'000}, H264LevelLimit{52, 240'000},
    H264LevelLimit{60, 240'000}, H264LevelLimit{61, 480'000},
    H264LevelLimit{62, 800'000}};

struct HevcLevelLimit final {
    std::uint8_t levelIdc;
    std::uint64_t mainTierCpbKilobits;
    std::uint64_t highTierCpbKilobits;
};

// ITU-T H.265 (08/2021), Annex A level limits. A zero high-tier value means
// that High tier is not defined for that level.
constexpr std::array HevcLevelLimits{
    HevcLevelLimit{30, 350, 0}, HevcLevelLimit{60, 1'500, 0},
    HevcLevelLimit{63, 3'000, 0}, HevcLevelLimit{90, 6'000, 0},
    HevcLevelLimit{93, 10'000, 0}, HevcLevelLimit{120, 12'000, 30'000},
    HevcLevelLimit{123, 20'000, 50'000},
    HevcLevelLimit{150, 25'000, 100'000},
    HevcLevelLimit{153, 40'000, 160'000},
    HevcLevelLimit{156, 60'000, 240'000},
    HevcLevelLimit{180, 60'000, 240'000},
    HevcLevelLimit{183, 120'000, 480'000},
    HevcLevelLimit{186, 240'000, 800'000}};

::media::Result<std::uint64_t> cpbBytes(
    std::uint64_t kilobits,
    std::uint64_t profileScaleNumerator,
    std::uint64_t profileScaleDenominator,
    const char* owner)
{
    auto bits = MediaCheckedArithmetic::ceilScale(
        kilobits, profileScaleNumerator * 1'000U,
        profileScaleDenominator, owner);
    return bits
        ? MediaCheckedArithmetic::ceilScale(bits.value(), 1, 8, owner)
        : bits;
}

::media::Result<std::pair<std::uint64_t, std::uint64_t>>
h264ProfileScale(std::uint8_t profileIdc)
{
    switch (profileIdc) {
    case 66:
    case 77:
    case 88:
        return ::media::Result<std::pair<std::uint64_t, std::uint64_t>>::success(
            {1, 1});
    case 100:
        return ::media::Result<std::pair<std::uint64_t, std::uint64_t>>::success(
            {5, 4});
    case 110:
        return ::media::Result<std::pair<std::uint64_t, std::uint64_t>>::success(
            {3, 1});
    case 122:
    case 244:
    case 44:
        return ::media::Result<std::pair<std::uint64_t, std::uint64_t>>::success(
            {4, 1});
    default:
        return ::media::Result<std::pair<std::uint64_t, std::uint64_t>>::failure(
            ::media::ErrorInfo::unsupported(
                "H264 RTP profile has no admitted H.264 Annex A CPB scale"));
    }
}

::media::Result<std::uint64_t> h264MaximumAccessUnitBytes(
    const MediaH264SignalingFacts& facts)
{
    auto profile = decodeRtpFmtpHex(facts.profileLevelId);
    if (!profile || profile.value().size() != 3) {
        return ::media::Result<std::uint64_t>::failure(
            profile ? ::media::ErrorInfo::invalidArgument(
                          "H264 RTP profile-level-id is malformed")
                    : profile.error());
    }
    const auto profileIdc = profile.value()[0];
    const auto constraints = profile.value()[1];
    const auto levelIdc = profile.value()[2];
    auto scale = h264ProfileScale(profileIdc);
    if (!scale) return ::media::Result<std::uint64_t>::failure(scale.error());

    std::uint64_t maximumCpbKilobits = 0;
    const bool level1b = levelIdc == 11 && (constraints & 0x10U) != 0;
    if (level1b) {
        maximumCpbKilobits = 350;
    } else {
        for (const auto& limit : H264LevelLimits) {
            if (limit.levelIdc == levelIdc) {
                maximumCpbKilobits = limit.maximumCpbKilobits;
                break;
            }
        }
    }
    if (maximumCpbKilobits == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "H264 RTP level has no admitted H.264 Annex A MaxCPB bound"));
    }
    auto cpb = cpbBytes(maximumCpbKilobits, scale.value().first,
                        scale.value().second, "H264 Annex A MaxCPB");
    return cpb
        ? MediaCheckedArithmetic::multiply(
              cpb.value(), H264AnnexBExpansionNumerator,
              "H264 depacketized Annex-B access-unit bound")
        : cpb;
}

::media::Result<std::vector<std::uint8_t>> rbsp(
    std::span<const std::uint8_t> ebsp)
{
    std::vector<std::uint8_t> output;
    try {
        output.reserve(ebsp.size());
        unsigned int consecutiveZeros = 0;
        for (const auto byte : ebsp) {
            if (consecutiveZeros == 2 && byte == 0x03U) {
                consecutiveZeros = 0;
                continue;
            }
            output.push_back(byte);
            consecutiveZeros = byte == 0 ? consecutiveZeros + 1 : 0;
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::allocationFailed("HEVC SPS RBSP"));
    }
    return ::media::Result<std::vector<std::uint8_t>>::success(
        std::move(output));
}

::media::Result<std::uint64_t> hevcMaximumAccessUnitBytes(
    const MediaHevcSignalingFacts& facts)
{
    if (facts.sps.size() < 3) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC RTP SPS is truncated"));
    }
    auto bytes = rbsp(std::span<const std::uint8_t>(facts.sps).subspan(2));
    if (!bytes || bytes.value().size() < 13) {
        return ::media::Result<std::uint64_t>::failure(
            bytes ? ::media::ErrorInfo::invalidArgument(
                        "HEVC RTP SPS lacks general_profile_tier_level")
                  : bytes.error());
    }
    const std::uint8_t profileIdc = bytes.value()[1] & 0x1fU;
    const bool highTier = (bytes.value()[1] & 0x20U) != 0;
    const std::uint8_t levelIdc = bytes.value()[12];
    if (profileIdc != 1 && profileIdc != 2) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "HEVC RTP profile has no admitted Annex A CPB scale"));
    }
    std::uint64_t maximumCpbKilobits = 0;
    for (const auto& limit : HevcLevelLimits) {
        if (limit.levelIdc != levelIdc) continue;
        maximumCpbKilobits = highTier
            ? limit.highTierCpbKilobits
            : limit.mainTierCpbKilobits;
        break;
    }
    if (maximumCpbKilobits == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "HEVC RTP tier/level has no admitted H.265 Annex A MaxCPB bound"));
    }
    auto cpb = cpbBytes(maximumCpbKilobits, 1, 1,
                        "HEVC Annex A MaxCPB");
    return cpb
        ? MediaCheckedArithmetic::multiply(
              cpb.value(), HevcAnnexBExpansionNumerator,
              "HEVC depacketized Annex-B access-unit bound")
        : cpb;
}

::media::Result<MediaPreparedRtpAccessUnitEnvelope> planVideo(
    const MediaRtpDepacketizerConfig& depacketizer,
    const std::string& codec)
{
    auto signaling = parseRtpVideoSignalingFacts(codec, depacketizer.fmtp);
    if (!signaling) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            signaling.error());
    }
    auto maximum = codec == "h264"
        ? h264MaximumAccessUnitBytes(
              std::get<MediaH264SignalingFacts>(signaling.value()))
        : hevcMaximumAccessUnitBytes(
              std::get<MediaHevcSignalingFacts>(signaling.value()));
    if (!maximum) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            maximum.error());
    }
    return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::success({
        MediaStreamKind::Video, codec, maximum.value(), 1,
        codec == "h264"
            ? "ITU-T-H.264-Annex-A-MaxCPB+profile-scale+AnnexB-expansion"
            : "ITU-T-H.265-Annex-A-MaxCPB+tier+AnnexB-expansion",
        "non-interleaved-video-RTP-marker-completes-at-most-one-access-unit-per-push"});
}

::media::Result<MediaPreparedRtpAccessUnitEnvelope> planAudio(
    const MediaRtpDepacketizerConfig& depacketizer,
    const std::string& codec,
    std::uint64_t maximumDatagramBytes)
{
    if (maximumDatagramBytes <= RtpHeaderBytes) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP maximum datagram cannot carry a payload"));
    }
    const auto maximumPayload = maximumDatagramBytes - RtpHeaderBytes;
    if (codec == "opus") {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::success({
            MediaStreamKind::Audio, codec, maximumPayload, 1,
            "RFC-7587-one-Opus-packet-per-RTP-payload+endpoint-maximum-datagram",
            "Opus-depacketizer-completes-one-access-unit-per-push"});
    }
    if (codec != "aac") {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            ::media::ErrorInfo::unsupported(
                "Raw RTP audio access-unit envelope codec is unsupported"));
    }
    auto parameters = parseRtpFmtp(depacketizer.fmtp);
    auto sizeLength = parameters
        ? requiredRtpFmtpInt(parameters.value(), "sizelength")
        : ::media::Result<int>::failure(parameters.error());
    if (!sizeLength || sizeLength.value() != AacSizeLengthBits) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            sizeLength ? ::media::ErrorInfo::unsupported(
                             "AAC RTP access-unit bound requires negotiated sizeLength=13")
                       : sizeLength.error());
    }
    const auto minimumPerAu = AacAuHeaderBytes + AacMinimumPayloadBytes;
    if (maximumPayload <= AacAuHeadersLengthBytes) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AAC RTP maximum datagram cannot carry an AU header and payload"));
    }
    const auto completions =
        (maximumPayload - AacAuHeadersLengthBytes) / minimumPerAu;
    if (completions == 0) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AAC RTP maximum datagram cannot complete an access unit"));
    }
    const std::uint64_t maximumAuBytes =
        (std::uint64_t{1} << AacSizeLengthBits) - 1;
    return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::success({
        MediaStreamKind::Audio, codec, maximumAuBytes, completions,
        "RFC-3640-negotiated-13-bit-AU-size-field",
        "RFC-3640-AU-header-geometry+endpoint-maximum-datagram"});
}

} // namespace

::media::Status MediaPreparedRtpAccessUnitEnvelope::validate() const
{
    if ((streamKind != MediaStreamKind::Video &&
         streamKind != MediaStreamKind::Audio) ||
        codecName.empty() || maximumAccessUnitBytes == 0 ||
        maximumAccessUnitsPerPush == 0 || sizeAuthority.empty() ||
        completionAuthority.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "prepared RTP access-unit envelope is incomplete"));
    }
    return ::media::Status::success();
}

MediaPreparedInputPayloadEnvelope
MediaPreparedRtpAccessUnitEnvelope::asInputPayloadEnvelope() const
{
    return {
        MediaPreparedInputPayloadSource::RawRtpAccessUnit,
        maximumAccessUnitsPerPush,
        completionAuthority,
        {{streamKind, maximumAccessUnitBytes, sizeAuthority}}};
}

::media::Result<MediaPreparedRtpAccessUnitEnvelope>
MediaPreparedRtpAccessUnitEnvelopePlanner::plan(
    const MediaRtpDepacketizerConfig& depacketizer,
    std::uint64_t maximumDatagramBytes)
{
    if (auto status = MediaRtpDepacketizerFactory::validate(depacketizer);
        !status) {
        return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
            status.error());
    }
    const auto codec = canonicalCodecName(depacketizer.codecName);
    auto result = depacketizer.streamKind == MediaStreamKind::Video
        ? planVideo(depacketizer, codec)
        : planAudio(depacketizer, codec, maximumDatagramBytes);
    if (result) {
        if (auto status = result.value().validate(); !status) {
            return ::media::Result<MediaPreparedRtpAccessUnitEnvelope>::failure(
                status.error());
        }
    }
    return result;
}

} // namespace media::ffmpeg::graph
