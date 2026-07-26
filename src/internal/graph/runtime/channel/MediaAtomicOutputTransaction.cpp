#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"

#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

#include <algorithm>
#include <new>
#include <unordered_map>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalidContract(const std::string& owner)
{
    return ::media::ErrorInfo::invalidArgument(
        owner + " requires bounded FIFO blocking ordered outputs");
}

} // namespace

MediaAtomicOutputTransaction::MediaAtomicOutputTransaction(
    std::string owner,
    std::vector<OwnedBatch> batches,
    std::vector<MediaChannel*> channels,
    std::vector<std::unique_lock<std::mutex>> channelLocks,
    std::vector<std::unique_lock<std::mutex>> queueLocks)
    : m_owner(std::move(owner))
    , m_batches(std::move(batches))
    , m_channels(std::move(channels))
    , m_channelLocks(std::move(channelLocks))
    , m_queueLocks(std::move(queueLocks))
{
}

MediaAtomicOutputTransaction::AcquireResult
MediaAtomicOutputTransaction::acquire(
    const char* owner,
    std::span<const MediaAtomicOutputBatch> batches)
{
    const std::string ownerName = owner ? owner : "Atomic output transaction";
    if (batches.empty()) {
        return AcquireResult::failure(::media::ErrorInfo::invalidArgument(
            ownerName + " requires explicit output batches"));
    }

    std::vector<MediaChannel*> channels;
    channels.reserve(batches.size());
    for (const auto& batch : batches) {
        if (!batch.channel) {
            return AcquireResult::failure(
                ::media::ErrorInfo::notInitialized(
                    ownerName + " requires explicit outputs"));
        }
        channels.push_back(batch.channel);
    }
    std::sort(channels.begin(), channels.end(), [](MediaChannel* lhs,
                                                    MediaChannel* rhs) {
        if (lhs->id() != rhs->id()) return lhs->id() < rhs->id();
        return std::less<MediaChannel*>{}(lhs, rhs);
    });
    channels.erase(std::unique(channels.begin(), channels.end()),
                   channels.end());

    std::unordered_map<MediaChannel*, std::size_t> required;
    required.reserve(channels.size());
    std::vector<OwnedBatch> ownedBatches;
    ownedBatches.reserve(batches.size());
    for (const auto& batch : batches) {
        auto* queue = dynamic_cast<MediaBlockingQueue*>(
            batch.channel->m_queue.get());
        if (!queue) {
            return AcquireResult::failure(invalidContract(ownerName));
        }
        auto prepared = queue->preparePush(batch.buffers);
        if (!prepared) return AcquireResult::failure(prepared.error());
        required[batch.channel] += batch.buffers.size();
        ownedBatches.push_back(
            {batch.channel, queue, std::move(prepared).value()});
    }

    std::vector<std::unique_lock<std::mutex>> channelLocks;
    channelLocks.reserve(channels.size());
    for (MediaChannel* channel : channels) {
        channelLocks.emplace_back(channel->m_mutationMutex);
    }
    std::vector<std::unique_lock<std::mutex>> queueLocks;
    queueLocks.reserve(channels.size());
    for (MediaChannel* channel : channels) {
        auto* queue = static_cast<MediaBlockingQueue*>(
            channel->m_queue.get());
        queueLocks.emplace_back(queue->m_mutex);
    }

    for (MediaChannel* channel : channels) {
        auto* queue = static_cast<MediaBlockingQueue*>(
            channel->m_queue.get());
        if (queue->m_aborted || queue->m_closed ||
            channel->m_closeRequested) {
            return AcquireResult::failure(::media::ErrorInfo::cancelled(
                ownerName + " output is closed"));
        }
        if (!MediaAtomicOutputPolicyContract::accepts(channel->m_policy)) {
            return AcquireResult::failure(invalidContract(ownerName));
        }
        const std::size_t capacity = queue->m_policy.capacity;
        const std::size_t count = required[channel];
        if (count > capacity || channel->m_reservedCapacity > capacity - count ||
            queue->m_queue.size() >
                capacity - count - channel->m_reservedCapacity) {
            return AcquireResult::success(std::nullopt);
        }
    }

    return AcquireResult::success(MediaAtomicOutputTransaction(
        ownerName,
        std::move(ownedBatches),
        std::move(channels),
        std::move(channelLocks),
        std::move(queueLocks)));
}

::media::Status MediaAtomicOutputTransaction::commit()
{
    if (m_committed) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " transaction can only commit once"));
    }
    publishPrepared();
    return ::media::Status::success();
}

void MediaAtomicOutputTransaction::commitReserved() noexcept
{
    publishPrepared();
}

void MediaAtomicOutputTransaction::publishPrepared() noexcept
{
    if (m_committed) return;
    for (auto& batch : m_batches) {
        const std::size_t count = batch.prepared.nodes.size();
        batch.queue->publishPreparedLocked(batch.prepared);
        batch.channel->m_metrics.pushed.fetch_add(
            count, std::memory_order_relaxed);
        batch.channel->refreshQueueMetrics();
    }
    m_committed = true;
    for (auto& lock : m_queueLocks) lock.unlock();
    for (auto& lock : m_channelLocks) lock.unlock();
    for (MediaChannel* channel : m_channels) {
        static_cast<MediaBlockingQueue*>(
            channel->m_queue.get())->notifyPreparedPublished();
        channel->publishAcceptedMutation();
    }
}

} // namespace media::ffmpeg::graph
