#include "internal/graph/planner/realtime/MediaTsPrefixPublicationPlanner.h"

#include "internal/graph/planner/realtime/MediaTsPrefixServicePlan.h"

#include <algorithm>
#include <sstream>

namespace media::ffmpeg::graph {

::media::Result<MediaTsPrefixPublicationDecision>
MediaTsPrefixPublicationPlanner::plan(
    MediaRunningTime prefixRelease,
    MediaRunningTime accessUnitDeadline,
    std::span<const MediaTsMaintenanceServiceObligation> maintenance,
    MediaRunningTime observedNow,
    const MediaTsPublishedWorkService& publishedWork,
    MediaRunningTime requiredServiceDuration,
    MediaTsPublicationOrder order)
{
    using Result = ::media::Result<MediaTsPrefixPublicationDecision>;
    if (prefixRelease.nanoseconds() < 0 ||
        accessUnitDeadline <= prefixRelease ||
        observedNow.nanoseconds() < 0 ||
        publishedWork.availableAt < observedNow ||
        requiredServiceDuration.nanoseconds() <= 0 ||
        (order != MediaTsPublicationOrder::YieldMediaToDueMaintenance &&
         order != MediaTsPublicationOrder::PreserveSelectedMaintenance))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix publication requires valid AU and maintenance timing facts"));

    // Test the original AU deadline before returning a maintenance boundary.
    // Otherwise a missed AU could be kept waiting indefinitely for maintenance.
    if (observedNow >= accessUnitDeadline) {
        std::ostringstream message;
        message << "MPEG-TS access unit expired before prefix publication"
                << " prefix_release_ns=" << prefixRelease.nanoseconds()
                << " au_deadline_ns=" << accessUnitDeadline.nanoseconds()
                << " observed_ns=" << observedNow.nanoseconds();
        return Result::failure(::media::ErrorInfo::invalidArgument(message.str()));
    }

    // Backward latest-start calculation for the supplied FIFO obligations.
    // This protects their deadlines under the service model; it does not
    // establish schedulability with unknown debt or OS scheduling delay.
    auto limit = accessUnitDeadline;
    if (!maintenance.empty()) {
        auto nextStart = maintenance.back().completionDeadline;
        for (std::size_t index = maintenance.size(); index != 0; --index) {
            const auto& obligation = maintenance[index - 1];
            if (obligation.release.nanoseconds() < 0 ||
                obligation.completionDeadline <= obligation.release ||
                obligation.serviceDuration.nanoseconds() <= 0 ||
                (index > 1 && maintenance[index - 2].release > obligation.release))
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "MPEG-TS maintenance requires ordered release, deadline and service facts"));
            auto start = (std::min)(nextStart, obligation.completionDeadline)
                .checkedSubtract(obligation.serviceDuration);
            if (!start) return Result::failure(start.error());
            if (start.value() < obligation.release)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "MPEG-TS imminent maintenance exceeds its original deadlines under conservative service admission"));
            nextStart = start.value();
        }
        limit = (std::min)(limit, nextStart);
    }
    auto earliestStart = (std::max)(prefixRelease, publishedWork.availableAt);
    // Accepted work includes conservative possible-report demand, not a
    // reservation of shared-scope capacity. Due checks still use real now.
    auto earliestCompletion = earliestStart.checkedAdd(requiredServiceDuration);
    if (!earliestCompletion) return Result::failure(earliestCompletion.error());
    if (earliestCompletion.value() > accessUnitDeadline) {
        std::ostringstream message;
        message << "MPEG-TS publication exceeds its original deadline under conservative service admission"
                << " release_ns=" << prefixRelease.nanoseconds()
                << " observed_ns=" << observedNow.nanoseconds()
                << " earliest_service_start_ns=" << earliestStart.nanoseconds()
                << " required_service_ns=" << requiredServiceDuration.nanoseconds()
                << " original_deadline_ns=" << accessUnitDeadline.nanoseconds();
        return Result::failure(::media::ErrorInfo::invalidArgument(message.str()));
    }
    if (!maintenance.empty() && order == MediaTsPublicationOrder::YieldMediaToDueMaintenance) {
        const auto nextRelease = maintenance.front().release;
        if (observedNow >= nextRelease || prefixRelease >= nextRelease ||
            earliestCompletion.value() > limit)
            return Result::success(MediaTsMaintenancePublicationBoundary{nextRelease});
    }
    if (earliestCompletion.value() > limit)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS selected maintenance exceeds successor limits under conservative service admission"));

    // This checks conservative serialization demand, not scheduling delays.
    // Sender completion must still be checked against both recorded limits.
    return Result::success(MediaTsPrefixPublicationWindow{
        prefixRelease, accessUnitDeadline, limit, publishedWork.observation});
}

::media::Result<MediaTsPrefixBatchDecision>
MediaTsPrefixPublicationPlanner::planBatch(
    const MediaTsUnpublishedTail& tail,
    MediaTsPrefixPublicationCapacity capacity,
    const MediaTsPrefixServicePlan& service,
    std::span<const MediaTsMaintenanceServiceObligation> maintenance,
    MediaRunningTime observedNow,
    const MediaTsPublishedWorkService& publishedWork)
{
    using Result = ::media::Result<MediaTsPrefixBatchDecision>;
    const auto width = service.maximumPayloadBytes();
    if (!width || !tail.payloadBytes || !tail.datagramCount ||
        (tail.payloadBytes - 1) / width + 1 != tail.datagramCount ||
        !service.maximumBatchDatagrams())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix batch requires the actual contiguous AU geometry"));

    const auto firstBytes = (std::min)(width, tail.payloadBytes);
    auto firstDuration = service.durationForPayload(firstBytes, 1);
    if (!firstDuration) return Result::failure(firstDuration.error());
    // Preserve the original single-datagram failure/yield before considering
    // batching. In particular, available capacity cannot bypass due maintenance.
    auto first = plan(tail.release, tail.deadline, maintenance, observedNow,
        publishedWork, firstDuration.value(),
        MediaTsPublicationOrder::YieldMediaToDueMaintenance);
    if (!first) return Result::failure(first.error());
    if (const auto* boundary = std::get_if<MediaTsMaintenancePublicationBoundary>(&first.value()))
        return Result::success(*boundary);
    const auto window = std::get<MediaTsPrefixPublicationWindow>(first.value());
    if (!capacity.availableHolders || capacity.availablePayloadBytes < firstBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS protocol batches exceed the planned queued byte capacity"));

    // The existing network ledger admits a complete shaping batch plus a
    // possible RTP report, including preparation/active/handoff and worst-case
    // wire partitions. Only mux-local free geometry is read here; global and
    // network snapshots are not reservations of transient capacity.
    auto maximum = (std::min)(tail.datagramCount, service.maximumBatchDatagrams());
    if (capacity.availablePayloadBytes < tail.payloadBytes)
        maximum = (std::min)(maximum, static_cast<std::size_t>(capacity.availablePayloadBytes / width));
    // The validated tail geometry proves count*width is representable for
    // every nonfinal prefix. The final prefix uses its actual short payload.
    const auto bytesFor = [&](std::size_t count) {
        return count == tail.datagramCount ? tail.payloadBytes : count * width;
    };
    const auto start = (std::max)(tail.release, publishedWork.availableAt);
    auto budget = window.completionLimit.checkedSubtract(start);
    if (!budget) return Result::failure(budget.error());
    std::size_t selected = 1;
    while (selected < maximum) {
        const auto distance = maximum - selected;
        const auto candidate = selected + distance / 2 + distance % 2;
        auto duration = service.durationForPayload(bytesFor(candidate), 1);
        if (!duration) return Result::failure(duration.error());
        // A typed duration comparison is infeasibility, not a suppressed error.
        // All candidates share the original release, accepted snapshot and
        // backward latest-start window, so positive per-datagram demand is monotone.
        if (duration.value() <= budget.value()) selected = candidate;
        else maximum = candidate - 1;
    }
    return Result::success(MediaTsPrefixBatch{selected, bytesFor(selected), window});
}

} // namespace media::ffmpeg::graph
