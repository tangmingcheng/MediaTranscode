#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/utils/MediaUrlUtils.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status validateClassification(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.type || !request.input.streamLayout || !request.output.streamLayout) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime input type and input/output stream layouts must be explicit"));
    }
    if (*request.input.type == RealtimeInputType::RtpPort &&
        *request.input.streamLayout == RealtimeInputStreamLayout::SeparateStreams &&
        *request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Synchronized separate RTP input to MPEG-TS output is not supported"));
    }
    const bool supported =
        (MediaRealtimeRequestClassifier::realtimeUrlInput(request) && MediaRealtimeRequestClassifier::separateRtpOutput(request)) ||
        (MediaRealtimeRequestClassifier::rawRtpInput(request) && MediaRealtimeRequestClassifier::separateRtpOutput(request)) ||
        (MediaRealtimeRequestClassifier::mpegTsUdpInput(request) && MediaRealtimeRequestClassifier::muxedTransportOutput(request));
    return supported
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::unsupported(
              "Realtime input type, input layout, and output layout combination is not supported"));
}

::media::Status validateQueues(const MediaGraphQueueParameters& queues)
{
    if (queues.metadata == 0 || queues.packet == 0 || queues.frame == 0 || queues.mux == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime queue capacities must be explicit and positive"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeRequestValidator::validate(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (auto status = validateClassification(request); !status) return status;
    if ((MediaRealtimeRequestClassifier::realtimeUrlInput(request) ||
         MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) && request.input.url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input URL must be explicit"));
    }
    if (MediaRealtimeRequestClassifier::separateRtpOutput(request)) {
        if (!request.output.packetSize || *request.output.packetSize <= 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP output packet size must be explicit and positive"));
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
    if (!request.input.openTimeoutMs || !request.input.readTimeoutMs ||
        !request.input.analyzeDurationUs || !request.input.probeSizeBytes || !request.input.lowLatency) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input requires explicit timeouts, probe, and latency options"));
    }
    auto output = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    return output ? ::media::Status::success() : ::media::Status::failure(output.error());
}

} // namespace media::ffmpeg::graph
