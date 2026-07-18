#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"

#include "internal/graph/runtime/channel/MediaChannel.h"

#include <algorithm>
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
    std::vector<std::unique_lock<std::mutex>> locks)
    : m_owner(std::move(owner))
    , m_batches(std::move(batches))
    , m_locks(std::move(locks))
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

    std::vector<std::unique_lock<std::mutex>> locks;
    locks.reserve(channels.size());
    for (MediaChannel* channel : channels) {
        locks.emplace_back(channel->m_mutationMutex);
    }

    std::unordered_map<MediaChannel*, std::size_t> required;
    required.reserve(channels.size());
    for (const auto& batch : batches) {
        for (const auto& buffer : batch.buffers) {
            if (!buffer) {
                return AcquireResult::failure(
                    ::media::ErrorInfo::invalidArgument(
                        ownerName + " rejects null output buffers"));
            }
        }
        required[batch.channel] += batch.buffers.size();
    }
    for (MediaChannel* channel : channels) {
        if (!channel->m_queue || channel->m_queue->aborted() ||
            channel->m_queue->closed()) {
            return AcquireResult::failure(::media::ErrorInfo::cancelled(
                ownerName + " output is closed"));
        }
        const auto& policy = channel->m_policy.queuePolicy;
        if (!policy.bounded || policy.capacity == 0 ||
            policy.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer ||
            policy.orderingPolicy != MediaQueueOrderingPolicy::Fifo ||
            !policy.preserveOrdering) {
            return AcquireResult::failure(invalidContract(ownerName));
        }
        const std::size_t capacity = channel->m_queue->capacity();
        const std::size_t count = required[channel];
        if (count > capacity || channel->m_queue->size() > capacity - count) {
            return AcquireResult::success(std::nullopt);
        }
    }

    std::vector<OwnedBatch> ownedBatches;
    ownedBatches.reserve(batches.size());
    for (const auto& batch : batches) {
        ownedBatches.push_back(OwnedBatch{
            batch.channel,
            std::vector<MediaBufferRef>(
                batch.buffers.begin(), batch.buffers.end())});
    }
    return AcquireResult::success(MediaAtomicOutputTransaction(
        ownerName, std::move(ownedBatches), std::move(locks)));
}

::media::Status MediaAtomicOutputTransaction::commit()
{
    if (m_committed) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " transaction can only commit once"));
    }
    for (const auto& batch : m_batches) {
        for (const auto& buffer : batch.buffers) {
            if (batch.channel->pushOutcomeLocked(buffer, false) !=
                MediaQueuePushOutcome::Accepted) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError(
                        m_owner + " transaction invariant was violated"));
            }
        }
    }
    for (auto& lock : m_locks) lock.unlock();
    std::vector<MediaChannel*> published;
    published.reserve(m_batches.size());
    for (const auto& batch : m_batches) {
        if (std::find(published.begin(), published.end(), batch.channel) ==
            published.end()) {
            batch.channel->publishAcceptedMutation();
            published.push_back(batch.channel);
        }
    }
    m_committed = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
