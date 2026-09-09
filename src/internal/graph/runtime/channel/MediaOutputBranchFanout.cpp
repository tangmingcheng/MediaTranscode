#include "internal/graph/runtime/channel/MediaOutputBranchFanout.h"
#include <algorithm>
#include <new>
#include <string>

namespace media::ffmpeg::graph {
::media::Status MediaOutputBranchFanout::subscribe(
    std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge,
    MediaBranchStartGate gate)
{
    if (!branch || (gate != MediaBranchStartGate::Immediate &&
                    gate != MediaBranchStartGate::RandomAccessUnit)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "output subscription requires a branch and explicit start gate"));
    }
    const auto inputs = branch->inputEdges();
    if (std::find(inputs.begin(), inputs.end(), edge) == inputs.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "output subscription requires a declared external edge"));
    }
    std::lock_guard lock(m_mutex);
    if (std::any_of(m_subscriptions.begin(), m_subscriptions.end(),
                   [&branch](const auto& subscription) {
                       return subscription.branch->id() == branch->id();
                   })) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "output branch is already subscribed"));
    }
    m_subscriptions.push_back({std::move(branch), edge, gate,
                              gate == MediaBranchStartGate::RandomAccessUnit,
                              ::media::ErrorInfo::allocationFailed("Output branch replica allocation failed")});
    return ::media::Status::success();
}

void MediaOutputBranchFanout::unsubscribe(std::uint64_t outputId)
{
    // Return only after any publication for this subscription has completed.
    std::lock_guard lock(m_mutex);
    std::erase_if(m_subscriptions, [outputId](const auto& subscription) {
        return subscription.branch->id() == outputId;
    });
}

::media::Status MediaOutputBranchFanout::publish(
    const MediaBufferRef& source, MediaBranchPublicationBoundary boundary,
    ReplicaFactory replica)
{
    if (!source || !replica ||
        (boundary != MediaBranchPublicationBoundary::Ordinary &&
         boundary != MediaBranchPublicationBoundary::RandomAccessUnit &&
         boundary != MediaBranchPublicationBoundary::Control)) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("fanout requires payload and replica contract"));
    std::lock_guard lock(m_mutex);
    for (auto& subscription : m_subscriptions) {
        if (subscription.branch->state() != MediaRuntimeBranchState::Running) continue;
        if (source->isFlush() && subscription.gate == MediaBranchStartGate::RandomAccessUnit)
            subscription.awaitingRandomAccess = true;
        if (subscription.awaitingRandomAccess) {
            if (boundary == MediaBranchPublicationBoundary::Ordinary) continue;
            if (boundary == MediaBranchPublicationBoundary::RandomAccessUnit)
                subscription.awaitingRandomAccess = false;
        }
        bool allocationFailed = false;
        auto copy = [&]() -> ::media::Result<MediaBufferRef> {
            try {
                return replica(source);
            } catch (const std::bad_alloc&) {
                // Prepared before publication so the exception path need not
                // allocate an error string. Other semantic exceptions propagate.
                allocationFailed = true;
                subscription.branch->fail(std::move(subscription.replicaAllocationFailure));
                return ::media::Result<MediaBufferRef>::success({});
            }
        }();
        if (allocationFailed) continue;
        if (!copy) { subscription.branch->fail(copy.error()); continue; }
        const auto publication = subscription.branch->tryPublish(
            subscription.edge, std::move(copy).value());
        if (publication.outcome == MediaQueuePushOutcome::WouldBlock ||
            publication.outcome == MediaQueuePushOutcome::Dropped ||
            publication.outcome == MediaQueuePushOutcome::Aborted) {
            const auto outcomeName = publication.outcome == MediaQueuePushOutcome::WouldBlock
                ? "WouldBlock" : publication.outcome == MediaQueuePushOutcome::Dropped
                ? "Dropped" : "Aborted";
            auto detail = std::string("output branch publication rejected edge=") +
                std::to_string(subscription.edge.value) + " capacity=" +
                std::to_string(publication.capacity) + " queued=" +
                std::to_string(publication.queued) + " outcome=" + outcomeName;
            subscription.branch->fail(publication.outcome == MediaQueuePushOutcome::Aborted
                ? ::media::ErrorInfo::internalError(std::move(detail))
                : ::media::ErrorInfo::wouldBlock(std::move(detail)));
        }
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
