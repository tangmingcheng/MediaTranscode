#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"

#include <chrono>
#include <limits>

namespace media::ffmpeg::graph {

namespace {

::media::Result<MediaRtpIngressPlan> planSnapshot(
    const MediaPreparedRtpIngressSnapshot& snapshot, const MediaRtpInputWaitPlan& inputWait)
{
    const auto& geometry = snapshot.receiveGeometry;
    if (geometry.channel() != MediaRtpUdpChannel::Rtp ||
        geometry.socketReceiveCapacityBytes() != snapshot.socketReceiveCapacityBytes ||
        geometry.maximumDatagramBytes() != snapshot.maximumDatagramBytes ||
        geometry.batchByteCapacity() > snapshot.preparedWireBytes)
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP snapshot differs from its original receive geometry"));
    return MediaRtpIngressPlan::create(geometry, snapshot.observation, inputWait);
}

} // namespace

::media::Result<MediaPreparedRtpIngressCandidate> MediaPreparedRtpIngressPlanner::candidate(
    const MediaPreparedRealtimeInput& input, const MediaRtpInputWaitPlan& inputWait,
    MediaRtpIngressSnapshotWait wait)
{
    using Result = ::media::Result<MediaPreparedRtpIngressCandidate>;
    if (inputWait.mode() != MediaRtpInputWaitMode::DeclaredInputLatency)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Live ingress candidate requires declared source input latency"));
    auto snapshot = input.rawRtpIngressSnapshot(wait);
    if (!snapshot) return Result::failure(snapshot.error());
    auto product = planSnapshot(snapshot.value(), inputWait);
    if (!product) return Result::failure(product.error());
    return Result::success({std::move(snapshot).value(), std::move(product).value()});
}

::media::Result<MediaRtpIngressPlan> MediaPreparedRtpIngressPlanner::plan(
    MediaPreparedRealtimeInput& input, const MediaRtpInputWaitPlan& inputWait,
    const MediaPreparedRtpIngressCandidate* candidate, MediaPreparedRtpIngressPhase phase)
{
    using Result = ::media::Result<MediaRtpIngressPlan>;
    const bool declared = inputWait.mode() == MediaRtpInputWaitMode::DeclaredInputLatency;
    if (declared != static_cast<bool>(candidate))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Final RTP ingress candidate presence conflicts with its selected waiting contract"));
    if (phase == MediaPreparedRtpIngressPhase::SealedCapture) {
        // The finite probe path retains its original stop/join evidence gate.
        auto sealed = input.rawRtpIngressObservation();
        if (!sealed) return Result::failure(sealed.error());
    } else if (phase == MediaPreparedRtpIngressPhase::ContinuousSession && candidate) {
        if (auto status = input.rawRtpCaptureStatus(); !status) return Result::failure(status.error());
    } else return Result::failure(::media::ErrorInfo::invalidArgument(
        "Continuous ingress finalization requires its installed source candidate"));
    auto snapshot = input.rawRtpIngressSnapshot(std::nullopt);
    if (!snapshot) return Result::failure(snapshot.error());
    if (candidate) {
        const auto& before = candidate->snapshot;
        const auto& after = snapshot.value();
        auto sourceBefore = before.source.lock();
        auto sourceAfter = after.source.lock();
        if (!sourceBefore || !sourceAfter || sourceBefore.get() != sourceAfter.get() ||
            sourceBefore.owner_before(sourceAfter) || sourceAfter.owner_before(sourceBefore) ||
            after.ingressFrontier < before.ingressFrontier ||
            after.socketReceiveCapacityBytes != before.socketReceiveCapacityBytes ||
            after.preparedWireBytes != before.preparedWireBytes ||
            after.maximumDatagramBytes != before.maximumDatagramBytes ||
            !after.receiveGeometry.sameProduct(before.receiveGeometry) ||
            after.observation.observedDatagrams() < before.observation.observedDatagrams() ||
            after.observation.observationSpanNanoseconds() < before.observation.observationSpanNanoseconds() ||
            after.observation.maximumDatagramBytes() < before.observation.maximumDatagramBytes() ||
            after.observation.maximumSequenceDisplacementPackets() < before.observation.maximumSequenceDisplacementPackets() ||
            after.observation.maximumInterarrivalNanoseconds() < before.observation.maximumInterarrivalNanoseconds())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Final RTP ingress evidence changed owner/capacity or regressed cumulative observations"));
    }
    auto product = planSnapshot(snapshot.value(), inputWait);
    if (!product) return Result::failure(product.error());
    if (candidate && !product.value().sameProduct(candidate->plan))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Final RTP ingress product differs from its observed source candidate"));
    return product;
}

::media::Status MediaPreparedRtpIngressPlanner::bind(
    const MediaRtpIngressPlan& ingress,
    MediaRealtimeRtpTransportPlan& transport)
{
    if (auto status = ingress.validateProduct(); !status) return status;
    if (!transport.inputWait) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("RTP transport has no selected input waiting contract"));
    if (auto status = transport.inputWait->validateFinalDelay(
            ingress.maximumReorderDelayNanoseconds()); !status) return status;
    const auto delay = std::chrono::ceil<std::chrono::milliseconds>(
        std::chrono::nanoseconds(ingress.maximumReorderDelayNanoseconds()));
    constexpr auto maximum = (std::numeric_limits<int>::max)();
    if (ingress.socketReceiveCapacityBytes() > static_cast<std::size_t>(maximum) ||
        ingress.maximumDatagramBytes() > static_cast<std::size_t>(maximum) ||
        ingress.reorderWindowPackets() > static_cast<std::size_t>(maximum) ||
        delay.count() > maximum) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "planner-bound RTP ingress facts exceed node option range"));
    }
    transport.ingress = ingress;
    transport.receiveBufferBytes = static_cast<int>(ingress.socketReceiveCapacityBytes());
    transport.maximumDatagramBytes = static_cast<int>(ingress.maximumDatagramBytes());
    transport.reorderWindowPackets = ingress.reorderWindowPackets();
    transport.maximumReorderDelayMs = static_cast<int>(delay.count());
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
