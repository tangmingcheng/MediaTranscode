#include "internal/graph/planner/realtime/MediaRealtimeRtpInputPlanValidator.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* field)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("Invalid raw RTP input product: ") + field));
}

} // namespace

::media::Status MediaRealtimeRtpInputPlanValidator::validate(
    RealtimeInputType inputType,
    const MediaRealtimeRtpInputNodePlan& input)
{
    const bool rawRtp = inputType == RealtimeInputType::RtpPort;
    if (rawRtp != input.rtpTransport.has_value() ||
        rawRtp != input.rtpDepacketizer.has_value()) {
        return invalid("transport and depacketizer presence");
    }
    if (!rawRtp) {
        if (input.requiresPreparedInput && *input.requiresPreparedInput) {
            return invalid("prepared ownership on non-RTP input");
        }
        return ::media::Status::success();
    }
    if (!input.requiresPreparedInput) {
        return invalid("prepared input ownership decision");
    }
    const auto& transport = *input.rtpTransport;
    if (transport.bindAddress.empty() || transport.rtpPort == 0 ||
        transport.rtcpPort == 0 || transport.rtcpPort != transport.rtpPort + 1 ||
        transport.payloadType > 127 || transport.clockRate <= 0 ||
        transport.receiveBufferBytes <= 0 ||
        transport.maximumDatagramBytes <= 0 ||
        transport.reorderWindowPackets == 0 ||
        transport.maximumReorderDelayMs <= 0 ||
        transport.cancellableReadTimeoutMs <= 0) {
        return invalid("transport identity or capacity");
    }
    if (!transport.requireSenderReports ||
        transport.senderReportTimeoutMs <= 0 ||
        transport.maximumExtrapolationMs <=
            transport.senderReportTimeoutMs ||
        transport.cnameTimeoutMs < transport.maximumExtrapolationMs) {
        return invalid("clock liveness deadlines");
    }
    if ((transport.clockLossPolicy !=
             MediaRtpClockLossPolicy::FailOnDegraded &&
         transport.clockLossPolicy !=
             MediaRtpClockLossPolicy::FailOnExpired) ||
        !transport.rtcpCompositionMode) {
        return invalid("clock loss or RTCP composition policy");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
