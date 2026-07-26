#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"

#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {

enum class MediaOutputCapacityReservationState {
    Pending,
    Authorized,
    Committed,
    Cancelled
};

struct MediaOutputCapacityReservationRecord final {
    struct ChannelCount final {
        MediaChannel* channel = nullptr;
        std::size_t count = 0;
    };

    std::mutex mutex;
    std::uint64_t identity = 0;
    MediaOutputCapacityReservationState state =
        MediaOutputCapacityReservationState::Pending;
    std::vector<ChannelCount> channels;
};

namespace {

std::atomic_uint64_t g_nextReservationIdentity{1};

std::vector<MediaChannel*> sortedChannels(
    const std::vector<MediaOutputCapacityReservationRecord::ChannelCount>& counts)
{
    std::vector<MediaChannel*> channels;
    channels.reserve(counts.size());
    for (const auto& count : counts) channels.push_back(count.channel);
    std::sort(channels.begin(), channels.end(), [](MediaChannel* lhs,
                                                    MediaChannel* rhs) {
        if (lhs->id() != rhs->id()) return lhs->id() < rhs->id();
        return std::less<MediaChannel*>{}(lhs, rhs);
    });
    channels.erase(std::unique(channels.begin(), channels.end()),
                   channels.end());
    return channels;
}

} // namespace

std::vector<std::unique_lock<std::mutex>>
MediaReservedOutputTransaction::lockChannels(
    const std::vector<MediaChannel*>& channels)
{
    std::vector<std::unique_lock<std::mutex>> locks;
    locks.reserve(channels.size());
    for (MediaChannel* channel : channels) {
        locks.emplace_back(channel->m_mutationMutex);
    }
    return locks;
}

MediaOutputCapacityReservationHandle::MediaOutputCapacityReservationHandle(
    std::shared_ptr<MediaOutputCapacityReservationRecord> record)
    : m_record(std::move(record))
{
}

bool MediaOutputCapacityReservationHandle::valid() const noexcept
{
    return static_cast<bool>(m_record);
}

std::uint64_t MediaOutputCapacityReservationHandle::identity() const noexcept
{
    return m_record ? m_record->identity : 0;
}

MediaReservedOutputTransaction::MediaReservedOutputTransaction(
    std::string owner,
    std::vector<OwnedBatch> batches,
    std::shared_ptr<MediaOutputCapacityReservationRecord> record)
    : m_owner(std::move(owner))
    , m_batches(std::move(batches))
    , m_record(std::move(record))
{
}

MediaReservedOutputTransaction::MediaReservedOutputTransaction(
    MediaReservedOutputTransaction&& other) noexcept
    : m_owner(std::move(other.m_owner))
    , m_batches(std::move(other.m_batches))
    , m_record(std::move(other.m_record))
{
}

MediaReservedOutputTransaction& MediaReservedOutputTransaction::operator=(
    MediaReservedOutputTransaction&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_owner = std::move(other.m_owner);
    m_batches = std::move(other.m_batches);
    m_record = std::move(other.m_record);
    return *this;
}

MediaReservedOutputTransaction::~MediaReservedOutputTransaction()
{
    cancel();
}

MediaReservedOutputTransaction::ReserveResult
MediaReservedOutputTransaction::reserve(
    const char* owner,
    std::span<const MediaAtomicOutputBatch> batches)
{
    const std::string ownerName = owner ? owner : "Reserved output transaction";
    if (batches.empty()) {
        return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
            ownerName + " requires explicit output batches"));
    }

    std::unordered_map<MediaChannel*, std::size_t> required;
    std::vector<OwnedBatch> ownedBatches;
    ownedBatches.reserve(batches.size());
    for (const auto& batch : batches) {
        if (!batch.channel) {
            return ReserveResult::failure(::media::ErrorInfo::notInitialized(
                ownerName + " requires explicit outputs"));
        }
        for (const auto& buffer : batch.buffers) {
            if (!buffer) {
                return ReserveResult::failure(
                    ::media::ErrorInfo::invalidArgument(
                        ownerName + " rejects null output buffers"));
            }
        }
        required[batch.channel] += batch.buffers.size();
        ownedBatches.push_back(OwnedBatch{
            batch.channel,
            std::vector<MediaBufferRef>(batch.buffers.begin(),
                                        batch.buffers.end())});
    }

    auto record = std::make_shared<MediaOutputCapacityReservationRecord>();
    record->identity = g_nextReservationIdentity.fetch_add(
        1, std::memory_order_relaxed);
    record->channels.reserve(required.size());
    for (const auto& [channel, count] : required) {
        record->channels.push_back({channel, count});
    }
    std::sort(record->channels.begin(), record->channels.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.channel->id() != rhs.channel->id())
                      return lhs.channel->id() < rhs.channel->id();
                  return std::less<MediaChannel*>{}(lhs.channel, rhs.channel);
              });
    const auto channels = sortedChannels(record->channels);
    auto locks = lockChannels(channels);
    for (const auto& item : record->channels) {
        MediaChannel* channel = item.channel;
        if (!channel->m_queue || channel->m_queue->aborted() ||
            channel->m_queue->closed() || channel->m_closeRequested) {
            return ReserveResult::failure(::media::ErrorInfo::cancelled(
                ownerName + " output is closed"));
        }
        if (!MediaAtomicOutputPolicyContract::accepts(channel->m_policy)) {
            return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
                ownerName +
                " requires bounded FIFO blocking ordered outputs"));
        }
        const std::size_t capacity = channel->m_queue->capacity();
        if (item.count > capacity || channel->m_reservedCapacity >
                capacity - item.count || channel->m_queue->size() >
                capacity - item.count - channel->m_reservedCapacity) {
            return ReserveResult::success(std::nullopt);
        }
    }
    for (const auto& item : record->channels) {
        item.channel->m_reservedCapacity += item.count;
    }
    return ReserveResult::success(MediaReservedOutputTransaction(
        ownerName, std::move(ownedBatches), std::move(record)));
}

MediaOutputCapacityReservationHandle
MediaReservedOutputTransaction::handle() const
{
    return MediaOutputCapacityReservationHandle(m_record);
}

::media::Status MediaReservedOutputTransaction::replacePendingBatches(
    std::span<const MediaAtomicOutputBatch> batches)
{
    if (!m_record || batches.size() != m_batches.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " replacement requires the reserved batch shape"));
    }
    std::lock_guard lock(m_record->mutex);
    if (m_record->state != MediaOutputCapacityReservationState::Pending) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " replacement requires a pending reservation"));
    }
    std::vector<OwnedBatch> replacement;
    replacement.reserve(batches.size());
    for (std::size_t index = 0; index < batches.size(); ++index) {
        const auto& batch = batches[index];
        const auto& reserved = m_batches[index];
        if (batch.channel != reserved.channel ||
            batch.buffers.size() != reserved.buffers.size()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                m_owner + " replacement changed reserved capacity"));
        }
        for (const auto& buffer : batch.buffers) {
            if (!buffer) return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    m_owner + " replacement rejects null buffers"));
        }
        replacement.push_back(OwnedBatch{
            batch.channel, std::vector<MediaBufferRef>(
                batch.buffers.begin(), batch.buffers.end())});
    }
    m_batches = std::move(replacement);
    return ::media::Status::success();
}

::media::Status MediaReservedOutputTransaction::authorize(
    std::span<const MediaOutputCapacityReservationHandle> handles,
    const Authorization& authorization)
{
    if (handles.empty() || !authorization) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reserved output authorization requires reservations and callback"));
    }
    std::vector<std::shared_ptr<MediaOutputCapacityReservationRecord>> records;
    records.reserve(handles.size());
    for (const auto& handle : handles) {
        if (!handle.m_record) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Reserved output authorization rejects an invalid handle"));
        }
        records.push_back(handle.m_record);
    }
    std::sort(records.begin(), records.end(), [](const auto& lhs,
                                                  const auto& rhs) {
        return lhs->identity < rhs->identity;
    });
    if (std::adjacent_find(records.begin(), records.end()) != records.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reserved output authorization rejects duplicate handles"));
    }
    std::vector<std::unique_lock<std::mutex>> recordLocks;
    recordLocks.reserve(records.size());
    for (const auto& record : records) recordLocks.emplace_back(record->mutex);

    std::vector<MediaOutputCapacityReservationRecord::ChannelCount> counts;
    for (const auto& record : records) {
        if (record->state != MediaOutputCapacityReservationState::Pending) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Reserved output authorization requires pending reservations"));
        }
        counts.insert(counts.end(), record->channels.begin(),
                      record->channels.end());
    }
    const auto channels = sortedChannels(counts);
    auto channelLocks = lockChannels(channels);
    for (const auto& record : records) {
        for (const auto& item : record->channels) {
            if (!item.channel->m_queue || item.channel->m_queue->aborted() ||
                item.channel->m_queue->closed() ||
                item.channel->m_closeRequested ||
                item.channel->m_reservedCapacity < item.count) {
                return ::media::Status::failure(::media::ErrorInfo::cancelled(
                    "Reserved output authorization found a closed output"));
            }
        }
    }
    if (auto status = authorization(); !status) return status;
    for (const auto& record : records) {
        record->state = MediaOutputCapacityReservationState::Authorized;
        for (const auto& item : record->channels) {
            item.channel->m_authorizedCapacity += item.count;
        }
    }
    return ::media::Status::success();
}

::media::Status MediaReservedOutputTransaction::commit(
    const Authorization& finalAuthorization)
{
    if (!m_record) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " reservation is not pending"));
    }
    std::unique_lock recordLock(m_record->mutex);
    if (m_record->state != MediaOutputCapacityReservationState::Authorized) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            m_owner + " reservation is not authorized"));
    }
    const auto channels = sortedChannels(m_record->channels);
    auto channelLocks = lockChannels(channels);
    for (const auto& item : m_record->channels) {
        if (!item.channel->m_queue || item.channel->m_queue->aborted()) {
            for (const auto& release : m_record->channels) {
                release.channel->m_reservedCapacity -= release.count;
                release.channel->m_authorizedCapacity -= release.count;
                release.channel->finalizeDeferredCloseLocked();
            }
            m_record->state = MediaOutputCapacityReservationState::Cancelled;
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                m_owner + " reservation was aborted"));
        }
    }
    if (finalAuthorization) {
        if (auto authorized = finalAuthorization(); !authorized) {
            return authorized;
        }
    }
    for (const auto& batch : m_batches) {
        for (const auto& buffer : batch.buffers) {
            if (batch.channel->pushReservedOutcomeLocked(buffer) !=
                MediaQueuePushOutcome::Accepted) {
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    m_owner + " reserved capacity invariant was violated"));
            }
        }
    }
    for (const auto& item : m_record->channels) {
        item.channel->m_reservedCapacity -= item.count;
        item.channel->m_authorizedCapacity -= item.count;
        item.channel->finalizeDeferredCloseLocked();
    }
    m_record->state = MediaOutputCapacityReservationState::Committed;
    recordLock.unlock();
    for (auto& lock : channelLocks) lock.unlock();
    for (MediaChannel* channel : channels) channel->publishAcceptedMutation();
    m_record.reset();
    m_batches.clear();
    return ::media::Status::success();
}

::media::Status MediaReservedOutputTransaction::cancel() noexcept
{
    if (!m_record) return ::media::Status::success();
    std::unique_lock recordLock(m_record->mutex);
    if (m_record->state == MediaOutputCapacityReservationState::Committed ||
        m_record->state == MediaOutputCapacityReservationState::Cancelled) {
        recordLock.unlock();
        m_record.reset();
        m_batches.clear();
        return ::media::Status::success();
    }
    const bool authorized =
        m_record->state == MediaOutputCapacityReservationState::Authorized;
    const auto channels = sortedChannels(m_record->channels);
    auto channelLocks = lockChannels(channels);
    for (const auto& item : m_record->channels) {
        item.channel->m_reservedCapacity -= item.count;
        if (authorized) item.channel->m_authorizedCapacity -= item.count;
        item.channel->finalizeDeferredCloseLocked();
    }
    m_record->state = MediaOutputCapacityReservationState::Cancelled;
    recordLock.unlock();
    for (auto& lock : channelLocks) lock.unlock();
    for (MediaChannel* channel : channels) {
        channel->publishReservedCapacityMutation();
    }
    m_record.reset();
    m_batches.clear();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
