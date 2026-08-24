#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"

#include "internal/graph/planner/MediaPipelineHardwareBackendConstraint.h"

#include "internal/graph/planner/MediaTranscodeStreamSetRequestValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/utils/MediaUrlUtils.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status validateClassification(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.type || !request.input.streamLayout || !request.output.streamLayout ||
        !request.output.transport) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime input type, input/output stream layouts, and output transport must be explicit"));
    }
    const bool supportedInput =
        MediaRealtimeRequestClassifier::realtimeUrlInput(request) ||
        MediaRealtimeRequestClassifier::rawRtpInput(request) ||
        MediaRealtimeRequestClassifier::mpegTsUdpInput(request);
    if (!supportedInput) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Realtime input type and input layout combination is not supported"));
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

::media::Status validateQueues(const MediaGraphQueueParameters& queues)
{
    if (queues.metadata == 0 || queues.packet == 0 || queues.frame == 0 || queues.mux == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime queue capacities must be explicit and positive"));
    }
    return ::media::Status::success();
}

bool rawRtpAudioControlSpecified(
    const MediaRealtimeRtpInputMetadata& audio) noexcept
{
    return !audio.url.empty() ||
        !audio.codecName.empty() ||
        audio.payloadType.has_value() ||
        audio.clockRate.has_value() ||
        audio.channels.has_value() ||
        audio.bitrateKbps.has_value() ||
        audio.fmtp.has_value();
}

bool preparedHandoffControlSpecified(
    const MediaRealtimePreparedHandoffConfig& handoff) noexcept
{
    return handoff.videoPacketCapacity.has_value() ||
        handoff.audioPacketCapacity.has_value() ||
        handoff.videoByteCapacity.has_value() ||
        handoff.audioByteCapacity.has_value();
}

::media::Status validatePreparedHandoffControls(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    const bool required =
        MediaRealtimeRequestClassifier::realtimeUrlInput(request) &&
        request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
    const bool specified = preparedHandoffControlSpecified(request.preparedHandoff);
    if (!required && specified) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared generic-input handoff bounds are valid only for AudioVideo session input"));
    }
    if (required && !specified) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioVideo session input requires explicit prepared handoff bounds"));
    }
    return ::media::Status::success();
}

::media::Status validateStreamSetControls(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.avSyncStartup.maximumVideoUnitBytes ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime input requires an explicit positive video startup bound"));
    }

    switch (*request.parameters.execution.streamSet) {
    case MediaTranscodeStreamSet::AudioVideo:
        if (!request.avSyncStartup.maximumAudioUnitBytes ||
            *request.avSyncStartup.maximumAudioUnitBytes == 0 ||
            !request.avSyncStartup.maximumGap ||
            request.avSyncStartup.maximumGap->nanoseconds() <= 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "AudioVideo requires explicit positive audio startup and A/V gap bounds"));
        }
        return ::media::Status::success();
    case MediaTranscodeStreamSet::VideoOnly:
        if (request.avSyncStartup.maximumAudioUnitBytes ||
            request.avSyncStartup.maximumGap) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly rejects audio startup and A/V gap bounds"));
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

} // namespace

::media::Status MediaRealtimeRequestValidator::validate(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (auto status = MediaPipelineHardwareBackendConstraint::validate(
            request.parameters.execution.hardwareBackend,
            request.parameters.execution.disableHardware,
            "Realtime request");
        !status) {
        return status;
    }
    if (request.mediaId.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime media identity must be explicit"));
    }
    if (!request.deployment) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Realtime Datagram output requires an authoritative deployment envelope before DAG construction"));
    }
    if (auto status = MediaTranscodeStreamSetRequestValidator::validate(
            request.parameters);
        !status) {
        return status;
    }
    if (auto status = validateStreamSetControls(request); !status) return status;
    if (auto status = validateClassification(request); !status) return status;
    if (auto status = validatePreparedHandoffControls(request); !status) return status;
    if ((MediaRealtimeRequestClassifier::realtimeUrlInput(request) ||
         MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) && request.input.url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input URL must be explicit"));
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
    if (auto status = validateQueues(request.parameters.queues); !status) return status;
    if (MediaRealtimeRequestClassifier::realtimeUrlInput(request)) {
        if (isUnsupportedRealtimeInputUrl(request.input.url)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported("Realtime URL input does not accept raw RTP, UDP, or SDP URLs"));
        }
        if (request.input.rtspTransport.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime URL input requires explicit RTSP transport"));
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
        !request.input.analyzeDurationUs || !request.input.probeSizeBytes || !request.input.lowLatency) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input requires explicit timeouts, probe, and latency options"));
    }
    auto output = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    return output ? ::media::Status::success() : ::media::Status::failure(output.error());
}

} // namespace media::ffmpeg::graph
