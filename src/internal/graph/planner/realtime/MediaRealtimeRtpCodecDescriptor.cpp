#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"
#include "internal/graph/protocol/rtp/MediaOpusRtpCapability.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr int DynamicPayloadTypeMin = 96;
constexpr int DynamicPayloadTypeMax = 127;
constexpr int VideoClockRate = 90000;
constexpr int OpusClockRate = 48000;
std::string lowercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

::media::Result<int> plannedAacAccessUnitDuration(
    const MediaRealtimeRtpInputMetadata& metadata)
{
    if (!metadata.fmtp) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP AAC requires fmtp"));
    }
    auto fmtp = parseRtpFmtp(*metadata.fmtp);
    if (!fmtp) return ::media::Result<int>::failure(fmtp.error());
    const auto configEntry = fmtp.value().find("config");
    if (configEntry == fmtp.value().end() || configEntry->second.empty()) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP AAC fmtp requires config"));
    }
    auto config = decodeRtpFmtpHex(configEntry->second);
    if (!config) return ::media::Result<int>::failure(config.error());
    auto asc = parseAacAudioSpecificConfig(config.value());
    if (!asc) return ::media::Result<int>::failure(asc.error());
    if (!metadata.clockRate || *metadata.clockRate != asc.value().sampleRate ||
        !metadata.channels || *metadata.channels != asc.value().channels) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP AAC AudioSpecificConfig conflicts with planned clock rate or channels"));
    }
    return ::media::Result<int>::success(asc.value().frameSamples);
}

::media::Result<void> requireFmtpKeys(const MediaRealtimeRtpInputMetadata& metadata,
                                      const std::string& owner,
                                      const std::initializer_list<const char*> keys)
{
    if (!metadata.fmtp || metadata.fmtp->empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " requires fmtp"));
    }
    auto parameters = parseRtpFmtp(*metadata.fmtp);
    if (!parameters) return ::media::Result<void>::failure(parameters.error());
    for (const char* key : keys) {
        const auto found = parameters.value().find(lowercaseAscii(key));
        if (found == parameters.value().end() || found->second.empty()) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(owner + " fmtp requires " + std::string(key)));
        }
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateCommonMetadata(const MediaRealtimeRtpInputMetadata& metadata,
                                             const std::string& owner)
{
    if (metadata.url.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " requires RTP URL"));
    }
    if (metadata.codecName.empty() ||
        !metadata.payloadType.has_value() ||
        !metadata.clockRate.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " requires codec name, payload type, and clock rate"));
    }
    if (*metadata.payloadType < DynamicPayloadTypeMin || *metadata.payloadType > DynamicPayloadTypeMax) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " payload type must be dynamic range 96..127"));
    }
    if (*metadata.clockRate <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " clock rate must be positive"));
    }
    return ::media::Result<void>::success();
}

::media::Result<MediaRealtimeRtpCodecDescriptor> describeVideo(
    const MediaRealtimeRtpInputMetadata& metadata)
{
    if (auto status = validateCommonMetadata(metadata, "Raw RTP video"); !status) {
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(status.error());
    }
    if (*metadata.clockRate != VideoClockRate) {
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP video clock rate must be 90000"));
    }

    const std::string codec = canonicalCodecName(metadata.codecName);
    MediaRealtimeRtpCodecDescriptor descriptor;
    descriptor.streamKind = MediaStreamKind::Video;
    descriptor.codecName = codec;
    descriptor.clockRate = VideoClockRate;

    if (codec == "h264") {
        if (auto status = requireFmtpKeys(metadata,
                                          "Raw RTP H264",
                                          { "packetization-mode", "sprop-parameter-sets", "profile-level-id" }); !status) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(status.error());
        }
        descriptor.rtpEncodingName = "H264";
        descriptor.requiresFmtp = true;
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }
    if (codec == "hevc") {
        if (auto status = requireFmtpKeys(metadata, "Raw RTP HEVC", { "sprop-vps", "sprop-sps", "sprop-pps" }); !status) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(status.error());
        }
        descriptor.rtpEncodingName = "H265";
        descriptor.requiresFmtp = true;
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }

    return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
        ::media::ErrorInfo::invalidArgument("Raw RTP video codec is not supported: " + codec));
}

::media::Result<MediaRealtimeRtpCodecDescriptor> describeAudio(
    const MediaRealtimeRtpInputMetadata& metadata)
{
    if (auto status = validateCommonMetadata(metadata, "Raw RTP audio"); !status) {
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(status.error());
    }
    if (!metadata.channels.has_value() || *metadata.channels <= 0) {
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP audio requires positive channel count"));
    }

    const std::string codec = canonicalCodecName(metadata.codecName);
    MediaRealtimeRtpCodecDescriptor descriptor;
    descriptor.streamKind = MediaStreamKind::Audio;
    descriptor.codecName = codec;
    descriptor.channels = *metadata.channels;

    if (codec == "aac") {
        if (!metadata.fmtp || metadata.fmtp->empty()) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
                ::media::ErrorInfo::invalidArgument("Raw RTP AAC requires fmtp"));
        }
        descriptor.rtpEncodingName = "MPEG4-GENERIC";
        descriptor.clockRate = *metadata.clockRate;
        auto duration = plannedAacAccessUnitDuration(metadata);
        if (!duration) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(duration.error());
        }
        descriptor.accessUnitDurationRtpTicks = duration.value();
        descriptor.maximumAccessUnitDurationRtpTicks = duration.value();
        descriptor.audioProfile = MediaAudioProfile::knownAacLow();
        descriptor.requiresFmtp = true;
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }
    if (codec == "opus") {
        if (*metadata.clockRate != OpusClockRate) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
                ::media::ErrorInfo::invalidArgument("Raw RTP Opus clock rate must be 48000"));
        }
        if (auto status = validateOpusRtpMappingFamilyZeroChannels(*metadata.channels);
            !status) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(status.error());
        }
        descriptor.rtpEncodingName = "opus";
        descriptor.clockRate = OpusClockRate;
        descriptor.maximumAccessUnitDurationRtpTicks =
            MaximumOpusRtpAccessUnitSamples;
        descriptor.audioProfile = MediaAudioProfile::notApplicable();
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }

    return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
        ::media::ErrorInfo::invalidArgument("Raw RTP audio codec is not supported: " + codec));
}

} // namespace

::media::Result<MediaRealtimeRtpCodecDescriptor> MediaRealtimeRtpCodecRegistry::describe(
    MediaStreamKind streamKind,
    const MediaRealtimeRtpInputMetadata& metadata)
{
    if (streamKind == MediaStreamKind::Video) {
        return describeVideo(metadata);
    }
    if (streamKind == MediaStreamKind::Audio) {
        return describeAudio(metadata);
    }
    return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
        ::media::ErrorInfo::invalidArgument("RTP codec descriptor requires audio or video stream kind"));
}

} // namespace media::ffmpeg::graph
