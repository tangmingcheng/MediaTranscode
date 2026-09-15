#include "internal/graph/planner/realtime/MediaRealtimeRtpInputPlanValidator.h"

#include <chrono>
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
    if (*input.requiresPreparedInput && !transport.ingress) {
        return invalid("prepared input ingress product");
    }
    if (transport.ingress) {
        const auto& ingress = *transport.ingress;
        if (auto status = ingress.validateProduct(); !status) return status;
        const auto delay = std::chrono::ceil<std::chrono::milliseconds>(
            std::chrono::nanoseconds(ingress.maximumReorderDelayNanoseconds()));
        if (transport.receiveBufferBytes <= 0 ||
            static_cast<std::size_t>(transport.receiveBufferBytes) != ingress.socketReceiveCapacityBytes() ||
            transport.maximumDatagramBytes <= 0 ||
            static_cast<std::size_t>(transport.maximumDatagramBytes) != ingress.maximumDatagramBytes() ||
            transport.reorderWindowPackets != ingress.reorderWindowPackets() ||
            transport.maximumReorderDelayMs != delay.count()) {
            return invalid("transport disagrees with prepared ingress product");
        }
    }
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
             MediaRtpClockLossPolicy::FailOnExpired &&
         transport.clockLossPolicy !=
             MediaRtpClockLossPolicy::WaitForEvidence) ||
        !transport.rtcpCompositionMode) {
        return invalid("clock loss or RTCP composition policy");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
