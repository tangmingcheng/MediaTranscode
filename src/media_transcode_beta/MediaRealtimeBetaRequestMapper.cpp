#include "media_transcode_beta/MediaRealtimeBetaRequestMapper.h"

#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"
#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"

#include "internal/graph/model/MediaHardwareBackendRequest.h"
#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/RealtimeStreamLayout.h"

#include <string>

namespace media::beta {
namespace {

std::string codecName(mt_beta_video_codec codec)
{
    return codec == MT_BETA_VIDEO_CODEC_H264 ? "h264" : "hevc";
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
    const std::string& sessionOwnedSdpPath)
{
    if (sessionOwnedSdpPath.empty()) {
        return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::failure(
            ::media::ErrorInfo::invalidArgument("session-owned SDP path is required"));
    }

    const auto& profile = MediaRealtimeBetaFixedProfile::current();
    ffmpeg::graph::MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = config.mediaId();
    request.input.type = ffmpeg::graph::RealtimeInputType::RtpPort;
    request.input.streamLayout = profile.inputLayout;
    request.input.url = rtpUrl(config);
    request.input.openTimeoutMs = profile.openTimeoutMs;
    request.input.readTimeoutMs = profile.readTimeoutMs;
    request.input.analyzeDurationUs = profile.analyzeDurationUs;
    request.input.probeSizeBytes = profile.probeSizeBytes;
    request.input.lowLatency = profile.lowLatency;
    request.input.videoRtp.url = request.input.url;
    request.input.videoRtp.codecName = codecName(config.inputCodec());
    request.input.videoRtp.payloadType = static_cast<int>(config.inputPayloadType());
    request.input.videoRtp.clockRate = static_cast<int>(config.inputClockRate());

    request.parameters.execution.streamSet = profile.streamSet;
    request.parameters.execution.disableHardware = false;
#ifdef _WIN32
    request.parameters.execution.hardwareBackend = ffmpeg::graph::MediaHardwareBackendRequest::Auto;
#else
    request.parameters.execution.hardwareBackend = ffmpeg::graph::MediaHardwareBackendRequest::RKMPP;
#endif
    request.parameters.queues.metadata = profile.metadataQueue;
    request.parameters.queues.packet = profile.packetQueue;
    request.parameters.queues.frame = profile.frameQueue;
    request.parameters.queues.mux = profile.muxQueue;
    request.parameters.video.codecName = codecName(config.outputCodec());
    request.parameters.video.width = static_cast<int>(config.width());
    request.parameters.video.height = static_cast<int>(config.height());
    request.parameters.video.frameRate.numerator = static_cast<int>(config.frameRateNumerator());
    request.parameters.video.frameRate.denominator = static_cast<int>(config.frameRateDenominator());
    request.parameters.video.gop = static_cast<int>(config.gopFrames());

    const auto& rateControl = config.rateControl();
    if (rateControl.mode == MT_BETA_RATE_CONTROL_CBR) {
        request.parameters.video.rateControl = ffmpeg::graph::MediaRateControlMode::Cbr;
        request.parameters.video.bitrateKbps = rateControl.targetBitrateKbps;
    } else {
        request.parameters.video.rateControl = ffmpeg::graph::MediaRateControlMode::Vbr;
        request.parameters.video.bitrateKbps = rateControl.targetBitrateKbps;
        request.parameters.video.minBitrateKbps = rateControl.minimumBitrateKbps;
        request.parameters.video.maxBitrateKbps = rateControl.maximumBitrateKbps;
    }

    request.avSyncStartup.maximumVideoUnitBytes = profile.startupMaximumVideoUnitBytes;
    request.output.streamLayout = profile.outputLayout;
    request.output.transport = profile.outputTransport;
    request.output.host = config.destinationAddress();
    request.output.basePort = static_cast<std::size_t>(config.destinationPort());
    request.output.sdpPath = sessionOwnedSdpPath;
    request.output.packetSize = profile.mpegTsRtpPacketSizeBytes;
    return ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>::success(
        std::move(request));
}

} // namespace media::beta
