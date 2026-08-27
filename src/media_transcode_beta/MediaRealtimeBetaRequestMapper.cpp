#include "media_transcode_beta/MediaRealtimeBetaRequestMapper.h"

#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"
#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"
#include "media_transcode_beta/MediaRealtimeBetaTemporaryDescription.h"

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/RealtimeStreamLayout.h"

#include <string>
#include <utility>

namespace media::beta {
namespace {

::media::Result<std::string> codecName(mt_beta_video_codec codec)
{
    switch (codec) {
    case MT_BETA_VIDEO_CODEC_H264:
        return ::media::Result<std::string>::success("h264");
    case MT_BETA_VIDEO_CODEC_HEVC:
        return ::media::Result<std::string>::success("hevc");
    }
    return ::media::Result<std::string>::failure(
        ::media::ErrorInfo::invalidArgument("video codec is unsupported"));
}

std::string rtpUrl(const MediaRealtimeBetaOwnedConfig& config)
{
    const std::string host = config.addressFamily() == ffmpeg::graph::MediaIpAddressFamily::Ipv6
        ? "[" + config.bindAddress() + "]"
        : config.bindAddress();
    return "rtp://" + host + ":" + std::to_string(config.inputPort());
}

} // namespace

::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>
MediaRealtimeBetaRequestMapper::map(
    const MediaRealtimeBetaOwnedConfig& config,
    const MediaRealtimeBetaTemporaryDescription& outputDescription)
{
    const std::string& sessionOwnedSdpPath = outputDescription.path();
    if (sessionOwnedSdpPath.empty()) {
        return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::failure(
            ::media::ErrorInfo::invalidArgument("session-owned SDP path is required"));
    }

    const auto& profile = MediaRealtimeBetaFixedProfile::current();
    auto inputCodec = codecName(config.inputCodec());
    if (!inputCodec) {
        return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::failure(
            inputCodec.error());
    }
    auto outputCodec = codecName(config.outputCodec());
    if (!outputCodec) {
        return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::failure(
            outputCodec.error());
    }
    ffmpeg::graph::MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = config.mediaId();
    auto deployment = ffmpeg::graph::MediaRealtimeDeploymentEnvelope::decode(
        config.deployment().encode());
    if (!deployment) {
        return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::failure(
            deployment.error());
    }
    request.deployment = std::move(deployment).value();
    request.input.type = ffmpeg::graph::RealtimeInputType::RtpPort;
    request.input.streamLayout = profile.inputLayout;
    request.input.url = rtpUrl(config);
    request.input.openTimeoutMs = profile.openTimeoutMs;
    request.input.readTimeoutMs = profile.readTimeoutMs;
    request.input.analyzeDurationUs = profile.analyzeDurationUs;
    request.input.probeSizeBytes = profile.probeSizeBytes;
    request.input.lowLatency = profile.lowLatency;
    request.input.videoRtp.url = request.input.url;
    request.input.videoRtp.codecName = std::move(inputCodec).value();
    request.input.videoRtp.payloadType = static_cast<int>(config.inputPayloadType());
    request.input.videoRtp.clockRate = static_cast<int>(config.inputClockRate());

    request.parameters.execution.streamSet = profile.streamSet;
    request.parameters.execution.disableHardware = profile.disableHardware;
    request.parameters.execution.hardwareBackend = profile.hardwareBackend;
    request.parameters.video.codecName = std::move(outputCodec).value();
    request.parameters.video.width = static_cast<int>(config.width());
    request.parameters.video.height = static_cast<int>(config.height());
    request.parameters.video.frameRate.numerator = static_cast<int>(config.frameRateNumerator());
    request.parameters.video.frameRate.denominator = static_cast<int>(config.frameRateDenominator());
    request.parameters.video.gop = static_cast<int>(config.gopFrames());

    const auto& rateControl = config.rateControl();
    if (const auto* cbr = std::get_if<MediaRealtimeBetaOwnedConfig::CbrRateControl>(
            &rateControl)) {
        request.parameters.video.rateControl = ffmpeg::graph::MediaRateControlMode::Cbr;
        request.parameters.video.bitrateKbps = cbr->targetBitrateKbps;
        request.parameters.video.bufferSizeKbits = cbr->bufferSizeKbits;
    } else {
        const auto& vbr = std::get<MediaRealtimeBetaOwnedConfig::VbrRateControl>(
            rateControl);
        request.parameters.video.rateControl = ffmpeg::graph::MediaRateControlMode::Vbr;
        request.parameters.video.bitrateKbps = vbr.targetBitrateKbps;
        request.parameters.video.minBitrateKbps = vbr.minimumBitrateKbps;
        request.parameters.video.maxBitrateKbps = vbr.maximumBitrateKbps;
        if (vbr.bufferSizeKbits > 0) {
            request.parameters.video.bufferSizeKbits = vbr.bufferSizeKbits;
        }
    }

    request.output.streamLayout = profile.outputLayout;
    request.output.transport = profile.outputTransport;
    request.output.host = config.destinationAddress();
    request.output.basePort = static_cast<std::size_t>(config.destinationPort());
    request.output.sdpPath = sessionOwnedSdpPath;
    return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::success(
        std::move(request));
}

} // namespace media::beta
