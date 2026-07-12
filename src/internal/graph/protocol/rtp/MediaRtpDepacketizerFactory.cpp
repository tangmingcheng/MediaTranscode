#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"

#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaH264RtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaHevcRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaOpusRtpDepacketizer.h"

#include <algorithm>
#include <cctype>

namespace media::ffmpeg::graph {
namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

::media::Status requireKey(const MediaRtpFmtpParameters& parameters, const std::string& key)
{
    if (!parameters.contains(key)) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RTP depacketizer fmtp missing " + key));
    return ::media::Status::success();
}

::media::Status validateConfig(const MediaRtpDepacketizerConfig& config)
{
    if ((config.streamKind != MediaStreamKind::Video && config.streamKind != MediaStreamKind::Audio) ||
        config.codecName.empty() || config.payloadType < 96 || config.payloadType > 127 || config.clockRate <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP depacketizer requires planned stream, codec, dynamic payload type, and clock rate"));
    }
    return ::media::Status::success();
}

::media::Status validateBase64List(const std::string& text, const std::string& owner)
{
    std::size_t offset = 0;
    while (offset <= text.size()) {
        const std::size_t comma = text.find(',', offset);
        auto decoded = decodeRtpFmtpBase64(text.substr(
            offset, comma == std::string::npos ? std::string::npos : comma - offset));
        if (!decoded || decoded.value().empty()) {
            return ::media::Status::failure(decoded
                ? ::media::ErrorInfo::invalidArgument(owner + " parameter set is empty")
                : decoded.error());
        }
        if (comma == std::string::npos) break;
        offset = comma + 1;
    }
    return ::media::Status::success();
}

::media::Status requireZeroWhenPresent(const MediaRtpFmtpParameters& parameters,
                                       const std::string& key,
                                       const std::string& owner)
{
    if (!parameters.contains(key)) return ::media::Status::success();
    auto value = requiredRtpFmtpInt(parameters, key);
    if (!value) return ::media::Status::failure(value.error());
    if (value.value() != 0) return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(owner + " optional payload header is unsupported: " + key));
    return ::media::Status::success();
}

} // namespace

::media::Result<std::unique_ptr<MediaRtpDepacketizer>> MediaRtpDepacketizerFactory::create(
    const MediaRtpDepacketizerConfig& config)
{
    if (auto status = validate(config); !status) return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::failure(status.error());
    const std::string codec = lower(config.codecName);
    if (codec == "opus") return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::success(std::make_unique<MediaOpusRtpDepacketizer>(config));
    if (codec == "h264") return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::success(std::make_unique<MediaH264RtpDepacketizer>(config));
    if (codec == "hevc") return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::success(std::make_unique<MediaHevcRtpDepacketizer>(config));
    if (codec == "aac") return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::success(std::make_unique<MediaAacRtpDepacketizer>(config));
    return ::media::Result<std::unique_ptr<MediaRtpDepacketizer>>::failure(
        ::media::ErrorInfo::unsupported("RTP depacketizer codec is unsupported: " + codec));
}

::media::Status MediaRtpDepacketizerFactory::validate(const MediaRtpDepacketizerConfig& config)
{
    if (auto status = validateConfig(config); !status) return status;
    const std::string codec = lower(config.codecName);
    if (codec == "opus") {
        if (config.streamKind != MediaStreamKind::Audio || config.clockRate != 48000 || config.channels <= 0) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Opus RTP requires planned audio, 48000 Hz, and channels"));
        return ::media::Status::success();
    }
    auto fmtp = parseRtpFmtp(config.fmtp);
    if (!fmtp) return ::media::Status::failure(fmtp.error());
    if (codec == "h264") {
        auto mode = requiredRtpFmtpInt(fmtp.value(), "packetization-mode");
        if (!mode || mode.value() != 1) return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("H264 RTP requires packetization-mode=1; interleaved modes are unsupported"));
        for (const char* key : {"sprop-parameter-sets", "profile-level-id"}) if (auto status = requireKey(fmtp.value(), key); !status) return status;
        if (auto status = validateBase64List(fmtp.value().at("sprop-parameter-sets"), "H264 RTP"); !status) return status;
        auto profile = decodeRtpFmtpHex(fmtp.value().at("profile-level-id"));
        if (!profile || profile.value().size() != 3) return ::media::Status::failure(
            profile ? ::media::ErrorInfo::invalidArgument("H264 RTP profile-level-id must be exactly three bytes") : profile.error());
        return ::media::Status::success();
    }
    if (codec == "hevc") {
        if (auto mode = fmtp.value().find("tx-mode"); mode != fmtp.value().end() && lower(mode->second) != "srst") return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("HEVC RTP only supports non-interleaved SRST transmission"));
        for (const char* key : {"sprop-vps", "sprop-sps", "sprop-pps"}) if (auto status = requireKey(fmtp.value(), key); !status) return status;
        for (const char* key : {"sprop-vps", "sprop-sps", "sprop-pps"}) {
            if (auto status = validateBase64List(fmtp.value().at(key), std::string("HEVC RTP ") + key); !status) return status;
        }
        if (auto status = requireZeroWhenPresent(
                fmtp.value(), "sprop-max-don-diff", "HEVC RTP DONL"); !status) return status;
        return ::media::Status::success();
    }
    if (codec == "aac") {
        if (config.streamKind != MediaStreamKind::Audio || config.channels <= 0 ||
            config.accessUnitDurationRtpTicks <= 0) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AAC RTP requires planned audio channels and access-unit duration"));
        const auto mode = fmtp.value().find("mode");
        if (mode == fmtp.value().end() || lower(mode->second) != "aac-hbr") return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("AAC RTP requires mode=AAC-hbr"));
        for (const auto& [key, expected] : {std::pair{"sizelength", 13}, {"indexlength", 3}, {"indexdeltalength", 3}}) {
            auto value = requiredRtpFmtpInt(fmtp.value(), key);
            if (!value || value.value() != expected) return ::media::Status::failure(
                ::media::ErrorInfo::unsupported("AAC RTP AU header configuration is unsupported"));
        }
        for (const char* key : {
                 "ctsdeltalength", "dtsdeltalength", "randomaccessindication",
                 "streamstateindication", "auxiliarydatasizelength"}) {
            if (auto status = requireZeroWhenPresent(
                    fmtp.value(), key, "AAC MPEG4-GENERIC"); !status) return status;
        }
        const auto foundConfig = fmtp.value().find("config");
        if (foundConfig == fmtp.value().end()) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AAC RTP fmtp requires config"));
        auto decoded = decodeRtpFmtpHex(foundConfig->second);
        if (!decoded || decoded.value().empty()) return ::media::Status::failure(
            decoded ? ::media::ErrorInfo::invalidArgument("AAC RTP config is empty") : decoded.error());
        auto asc = parseAacAudioSpecificConfig(decoded.value());
        if (!asc) return ::media::Status::failure(asc.error());
        if (config.clockRate != asc.value().sampleRate || config.channels != asc.value().channels ||
            config.accessUnitDurationRtpTicks != asc.value().frameSamples) return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AAC RTP clock, channels, or AU duration conflicts with AudioSpecificConfig"));
        return ::media::Status::success();
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported("RTP depacketizer codec is unsupported: " + codec));
}

} // namespace media::ffmpeg::graph
