#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/utils/MediaUrlUtils.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status validateClassification(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.type || !request.output.streamLayout || !request.output.transport) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime input type, output stream layout, and output transport must be explicit"));
    }
    const bool supportedInput =
        MediaRealtimeRequestClassifier::realtimeUrlInput(request) ||
        MediaRealtimeRequestClassifier::rawRtpInput(request) ||
        MediaRealtimeRequestClassifier::mpegTsUdpInput(request);
    if (!supportedInput) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Realtime input type is not supported"));
    }
    const bool supportedOutput =
        (MediaRealtimeRequestClassifier::separateStreamsOutput(request) &&
         MediaRealtimeRequestClassifier::rtpAvpOutput(request)) ||
        (MediaRealtimeRequestClassifier::muxedTransportOutput(request) &&
         (MediaRealtimeRequestClassifier::udpOutput(request) ||
          MediaRealtimeRequestClassifier::rtpAvpOutput(request)));
    return supportedOutput
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::unsupported(
              "Realtime output layout and transport combination is not supported"));
}

bool rawRtpAudioControlSpecified(
    const MediaRealtimeRtpInputMetadata& audio) noexcept
{
    return !audio.url.empty() ||
        !audio.codecName.empty() ||
        audio.payloadType.has_value() ||
        audio.clockRate.has_value() ||
        audio.channels.has_value() ||
        audio.fmtp.has_value();
}

bool audioTranscodeControlSpecified(
    const MediaRealtimeAudioTranscodeParameters& audio) noexcept
{
    return !audio.codecName.empty() ||
        audio.rateControl != MediaRateControlMode::Auto ||
        audio.bitrateKbps.has_value() ||
        audio.minBitrateKbps.has_value() ||
        audio.maxBitrateKbps.has_value() ||
        audio.sampleRate.has_value() ||
        audio.channels.has_value();
}

::media::Status validateStreamSetControls(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.parameters.execution.streamSet) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime transcode stream set must be explicit"));
    }
    switch (*request.parameters.execution.streamSet) {
    case MediaTranscodeStreamSet::AudioVideo:
        return ::media::Status::success();
    case MediaTranscodeStreamSet::VideoOnly:
        if (audioTranscodeControlSpecified(request.parameters.audio)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly rejects explicit audio transcode controls"));
        }
        if (rawRtpAudioControlSpecified(request.input.audioRtp)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly rejects raw RTP audio controls"));
        }
        return ::media::Status::success();
    }

    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "Transcode stream set is not supported"));
}

::media::Status validateDeploymentFacts(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.deployment.provisionedEgressCapacityBitsPerSecond ||
        !request.deployment.maximumWireResidence ||
        *request.deployment.provisionedEgressCapacityBitsPerSecond < 8 ||
        *request.deployment.maximumWireResidence <=
            MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime Datagram deployment requires positive provisioned egress capacity and maximum wire residence facts"));
    }
    return ::media::Status::success();
}

::media::Status validateVideoEncodingIntent(
    const MediaRealtimeVideoTranscodeParameters& video)
{
    if (!video.gop || *video.gop <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime video GOP must be an explicit positive frame count"));
    }
    if (video.rateControl != MediaRateControlMode::Cbr &&
        video.rateControl != MediaRateControlMode::Vbr) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime video rate control must be explicit CBR or VBR"));
    }
    if (video.rateControl == MediaRateControlMode::Cbr) {
        if (!video.bitrateKbps || *video.bitrateKbps <= 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Realtime video CBR requires a positive target bitrate"));
        }
        if (video.minBitrateKbps || video.maxBitrateKbps) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Realtime video CBR accepts only target bitrate"));
        }
        return ::media::Status::success();
    }
    if (!video.minBitrateKbps || !video.bitrateKbps ||
        !video.maxBitrateKbps ||
        *video.minBitrateKbps <= 0 || *video.bitrateKbps <= 0 ||
        *video.maxBitrateKbps <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime video VBR requires positive minimum, target, and maximum bitrates"));
    }
    if (*video.minBitrateKbps > *video.bitrateKbps ||
        *video.bitrateKbps > *video.maxBitrateKbps) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime video VBR requires minimum <= target <= maximum bitrate"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeRequestValidator::validate(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (request.mediaId.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime media identity must be explicit"));
    }
    if (auto status = validateVideoEncodingIntent(request.parameters.video); !status) {
        return status;
    }
    if (auto status = validateStreamSetControls(request); !status) return status;
    if (auto status = validateClassification(request); !status) return status;
    if (auto status = validateDeploymentFacts(request); !status) return status;
    if ((MediaRealtimeRequestClassifier::realtimeUrlInput(request) ||
         MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) && request.input.url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime input URL must be explicit"));
    }
    if (MediaRealtimeRequestClassifier::rtpAvpOutput(request)) {
        if (request.output.host.empty() || !request.output.basePort) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP output host and base port must be explicit"));
        }
        if (request.output.sdpPath.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP SDP output path must be explicit"));
        }
    }
    if (MediaRealtimeRequestClassifier::realtimeUrlInput(request)) {
        if (isUnsupportedRealtimeInputUrl(request.input.url)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported("Realtime URL input does not accept raw RTP, UDP, or SDP URLs"));
        }
        if (isRtspUrl(request.input.url) && request.input.rtspTransport.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime URL input requires explicit RTSP transport"));
        }
        if (!isRtspUrl(request.input.url) && !request.input.rtspTransport.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Non-RTSP realtime URL input rejects RTSP transport"));
        }
    }
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request) && !isUdpUrl(request.input.url)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS UDP input requires udp:// URL"));
    }
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) {
        if (!request.input.mpegTsClock.maximumPcrGap ||
            request.input.mpegTsClock.maximumPcrGap->nanoseconds() <= 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS input requires an explicit positive maximum PCR gap"));
        }
    } else if (request.input.mpegTsClock.maximumPcrGap) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "maximum PCR gap is valid only for MPEG-TS input"));
    }
    if (MediaRealtimeRequestClassifier::rawRtpInput(request) &&
        request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        auto audio = MediaRealtimeRtpCodecRegistry::describe(
            MediaStreamKind::Audio, request.input.audioRtp);
        if (!audio) {
            return ::media::Status::failure(audio.error());
        }
        auto depacketizer = MediaRealtimeRtpCodecRegistry::planDepacketizerConfig(
            MediaStreamKind::Audio, request.input.audioRtp, audio.value());
        if (!depacketizer) {
            return ::media::Status::failure(depacketizer.error());
        }
    }
    if (!request.input.openTimeoutMs || !request.input.readTimeoutMs ||
        !request.input.analyzeDurationUs || !request.input.probeSizeBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime RTP input requires explicit timeouts and probe limits"));
    }
    auto output = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    return output ? ::media::Status::success() : ::media::Status::failure(output.error());
}

} // namespace media::ffmpeg::graph
