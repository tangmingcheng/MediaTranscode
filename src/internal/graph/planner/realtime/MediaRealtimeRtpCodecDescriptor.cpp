#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr int DynamicPayloadTypeMin = 96;
constexpr int DynamicPayloadTypeMax = 127;
constexpr int VideoClockRate = 90000;
constexpr int OpusClockRate = 48000;

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
        descriptor.rtpEncodingName = "H264";
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }
    if (codec == "hevc") {
        descriptor.rtpEncodingName = "H265";
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
        if (metadata.fmtp.empty()) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
                ::media::ErrorInfo::invalidArgument("Raw RTP AAC requires fmtp"));
        }
        descriptor.rtpEncodingName = "MPEG4-GENERIC";
        descriptor.clockRate = *metadata.clockRate;
        descriptor.requiresFmtp = true;
        return ::media::Result<MediaRealtimeRtpCodecDescriptor>::success(std::move(descriptor));
    }
    if (codec == "opus") {
        if (*metadata.clockRate != OpusClockRate) {
            return ::media::Result<MediaRealtimeRtpCodecDescriptor>::failure(
                ::media::ErrorInfo::invalidArgument("Raw RTP Opus clock rate must be 48000"));
        }
        descriptor.rtpEncodingName = "OPUS";
        descriptor.clockRate = OpusClockRate;
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
