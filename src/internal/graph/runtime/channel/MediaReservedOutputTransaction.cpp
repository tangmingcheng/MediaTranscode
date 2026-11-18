#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"

#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <string_view>
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

    using AllocationAllocator = MediaLineageAllocator<MediaOutputCapacityReservationRecord>;
    MediaOutputCapacityReservationRecord(MediaLineageAllocationScope owner,
        MediaFixedCapacityStorage<ChannelCount> counts, std::uint64_t id) noexcept
        : allocation(std::move(owner)), identity(id), channels(std::move(counts)) {}

    MediaLineageAllocationScope allocation;
    std::mutex mutex;
    const std::uint64_t identity;
    MediaOutputCapacityReservationState state =
        MediaOutputCapacityReservationState::Pending;
    const MediaFixedCapacityStorage<ChannelCount> channels;
};

::media::Result<MediaOutputReservationLayout> MediaReservedOutputTransaction::allocationLayout()
{
    using Record = MediaOutputCapacityReservationRecord;
    auto record = probeMediaSharedAllocationGeometry<Record>(std::type_identity<Record::AllocationAllocator>{});
    if (!record) return ::media::Result<MediaOutputReservationLayout>::failure(record.error());
    return ::media::Result<MediaOutputReservationLayout>::success({
        {1, sizeof(OwnedBatch), alignof(OwnedBatch)},
        {1, sizeof(Record::ChannelCount), alignof(Record::ChannelCount)},
        {1, sizeof(MediaChannel*), alignof(MediaChannel*)},
        {1, sizeof(std::unique_lock<std::mutex>), alignof(std::unique_lock<std::mutex>)},
        {1, sizeof(std::shared_ptr<Record>), alignof(std::shared_ptr<Record>)},
        {1, sizeof(MediaBufferRef), alignof(MediaBufferRef)},
        {1, sizeof(char), alignof(char)}, record.value()});
}

namespace {

std::atomic_uint64_t g_nextReservationIdentity{1};

class MediaReservedOutputLockScope final {
public:
    MediaReservedOutputLockScope(
        std::span<std::unique_lock<std::mutex>> channelLocks,
        std::span<std::unique_lock<std::mutex>> queueLocks) noexcept
        : m_channelLocks(channelLocks)
        , m_queueLocks(queueLocks)
    {
    }

    MediaReservedOutputLockScope(
        const MediaReservedOutputLockScope&) = delete;
    MediaReservedOutputLockScope& operator=(
        const MediaReservedOutputLockScope&) = delete;

    ~MediaReservedOutputLockScope()
    {
        unlock();
    }

    void unlock() noexcept
    {
        for (auto& lock : m_queueLocks) {
            if (lock.owns_lock()) lock.unlock();
        }
        for (auto& lock : m_channelLocks) {
            if (lock.owns_lock()) lock.unlock();
        }
        // The backing may move to the returned transaction after explicit unlock.
        m_queueLocks = {};
        m_channelLocks = {};
    }

private:
    std::span<std::unique_lock<std::mutex>> m_channelLocks;
    std::span<std::unique_lock<std::mutex>> m_queueLocks;
};

} // namespace

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
    MediaLineageAllocationScope allocation,
    MediaFixedCapacityStorage<char> owner,
    MediaFixedCapacityStorage<OwnedBatch> batches,
    MediaFixedCapacityStorage<MediaChannel*> channels,
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> channelLocks,
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> queueLocks,
    std::shared_ptr<MediaOutputCapacityReservationRecord> record) noexcept
    : m_allocation(std::move(allocation))
    , m_owner(std::move(owner))
    , m_batches(std::move(batches))
    , m_channels(std::move(channels))
    , m_channelLocks(std::move(channelLocks))
    , m_queueLocks(std::move(queueLocks))
    , m_record(std::move(record))
{
}

MediaReservedOutputTransaction::MediaReservedOutputTransaction(
    MediaReservedOutputTransaction&& other) noexcept
    : m_allocation(std::move(other.m_allocation))
    , m_owner(std::move(other.m_owner))
    , m_batches(std::move(other.m_batches))
    , m_channels(std::move(other.m_channels))
    , m_channelLocks(std::move(other.m_channelLocks))
    , m_queueLocks(std::move(other.m_queueLocks))
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
    m_channels = std::move(other.m_channels);
    m_channelLocks = std::move(other.m_channelLocks);
    m_queueLocks = std::move(other.m_queueLocks);
    m_record = std::move(other.m_record);
    m_allocation = std::move(other.m_allocation);
    return *this;
}

MediaReservedOutputTransaction::~MediaReservedOutputTransaction()
{
    cancel();
}

MediaReservedOutputTransaction::ReserveResult
MediaReservedOutputTransaction::reserve(
    const char* owner,
    std::span<const MediaAtomicOutputBatch> batches, const MediaOutputReservationStoragePlan& storage)
{
    auto allocation = storage.allocation();
    if (auto valid = MediaOutputReservationStoragePlanner::validate(storage, allocation); !valid)
        return ReserveResult::failure(valid.error());
    try {
    const std::string_view ownerName = owner ? owner : "Reserved output transaction";
    if (batches.empty()) {
        return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
            std::string(ownerName) + " requires explicit output batches"));
    }
    auto* resource = MediaLineageAllocator<std::byte>{allocation}.resource();
    if (!resource) return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
        "Reserved output transaction requires its allocation owner"));
    using Record = MediaOutputCapacityReservationRecord;
    MediaFixedCapacityStorage<Record::ChannelCount> counts(batches.size(), *resource);
    for (const auto& batch : batches) {
        if (!batch.channel) return ReserveResult::failure(::media::ErrorInfo::notInitialized(
            std::string(ownerName) + " requires explicit outputs"));
        auto* queue = dynamic_cast<MediaBlockingQueue*>(batch.channel->m_queue.get());
        if (!queue) return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
            std::string(ownerName) + " requires bounded FIFO blocking ordered outputs"));
        if (batch.buffers.size() > queue->capacity())
            return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
                std::string(ownerName) + " batch exceeds its prepared output queue storage"));
        counts.push_back({batch.channel, batch.buffers.size()});
    }
    auto ordered = counts.view();
    std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.channel->id() != rhs.channel->id()) return lhs.channel->id() < rhs.channel->id();
        return std::less<MediaChannel*>{}(lhs.channel, rhs.channel);
    });
    Record::ChannelCount* previous = nullptr;
    for (auto& item : counts) {
        if (previous && previous->channel == item.channel) {
            const auto capacity = item.channel->m_queue->capacity();
            if (previous->count > capacity || item.count > capacity - previous->count)
                return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
                    std::string(ownerName) + " batch exceeds its prepared output queue storage"));
            previous->count += item.count;
            item.channel = nullptr;
        } else {
            previous = &item;
        }
    }
    counts.eraseIf([](const auto& item) { return item.channel == nullptr; });
    if (storage.prepared()) {
        auto cost = MediaOutputReservationStoragePlanner::reservationBytes(*storage.prepared(),
            batches | std::views::transform([](const auto& batch) { return batch.buffers.size(); }),
            counts.size(), ownerName.size());
        if (!cost) return ReserveResult::failure(cost.error());
    }
    MediaFixedCapacityStorage<OwnedBatch> ownedBatches(batches.size(), *resource);
    for (const auto& batch : batches) {
        auto* queue = static_cast<MediaBlockingQueue*>(batch.channel->m_queue.get());
        auto prepared = queue->preparePush(batch.buffers, allocation);
        if (!prepared) return ReserveResult::failure(prepared.error());
        ownedBatches.push_back({batch.channel, queue, std::move(prepared).value()});
    }
    MediaFixedCapacityStorage<char> ownerStorage(ownerName.size(), *resource);
    for (const auto character : ownerName) ownerStorage.push_back(character);
    MediaFixedCapacityStorage<MediaChannel*> channels(counts.size(), *resource);
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> channelLocks(counts.size(), *resource);
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> queueLocks(counts.size(), *resource);
    for (const auto& item : counts) {
        channels.push_back(item.channel);
        channelLocks.push_back(std::unique_lock<std::mutex>{});
        queueLocks.push_back(std::unique_lock<std::mutex>{});
    }
    auto record = std::allocate_shared<Record>(Record::AllocationAllocator{allocation}, allocation,
        std::move(counts), g_nextReservationIdentity.fetch_add(1, std::memory_order_relaxed));
    MediaReservedOutputLockScope locks(channelLocks.view(), queueLocks.view());
    for (std::size_t index = 0; index < channels.size(); ++index)
        channelLocks[index] = std::unique_lock<std::mutex>(channels[index]->m_mutationMutex);
    for (const auto& item : record->channels) {
        MediaChannel* channel = item.channel;
        if (!channel->m_queue || channel->m_queue->aborted() ||
            channel->m_queue->closed() || channel->m_closeRequested)
            return ReserveResult::failure(::media::ErrorInfo::cancelled(
                std::string(ownerName) + " output is closed"));
        if (!MediaAtomicOutputPolicyContract::accepts(channel->m_policy))
            return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
                std::string(ownerName) + " requires bounded FIFO blocking ordered outputs"));
        const auto capacity = channel->m_queue->capacity();
        if (item.count > capacity)
            return ReserveResult::failure(::media::ErrorInfo::invalidArgument(
                std::string(ownerName) + " batch exceeds planned output capacity"));
        if (channel->m_reservedCapacity > capacity - item.count ||
            channel->m_queue->size() > capacity - item.count - channel->m_reservedCapacity)
            return ReserveResult::success(std::nullopt);
    }
    MediaReservedOutputTransaction transaction(std::move(allocation), std::move(ownerStorage),
        std::move(ownedBatches), std::move(channels), std::move(channelLocks), std::move(queueLocks), std::move(record));
    for (const auto& item : transaction.m_record->channels)
        item.channel->m_reservedCapacity += item.count;
    locks.unlock();
    return ReserveResult::success(std::move(transaction));
    } catch (const std::bad_alloc&) {
        return ReserveResult::failure(::media::ErrorInfo::internalError(
            "Reserved output transaction allocation failed"));
    } catch (const std::length_error&) {
        return ReserveResult::failure(::media::ErrorInfo::internalError(
            "Reserved output transaction storage size is not representable"));
    }
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
            std::string(m_owner.begin(), m_owner.end()) + " replacement requires the reserved batch shape"));
    }
    std::lock_guard lock(m_record->mutex);
    if (m_record->state != MediaOutputCapacityReservationState::Pending) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            std::string(m_owner.begin(), m_owner.end()) + " replacement requires a pending reservation"));
    }
    for (std::size_t index = 0; index < batches.size(); ++index) {
        const auto& batch = batches[index];
        auto& reserved = m_batches[index];
        if (batch.channel != reserved.channel ||
            batch.buffers.size() != reserved.prepared.size()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                std::string(m_owner.begin(), m_owner.end()) + " replacement changed reserved capacity"));
        }
        for (const auto& buffer : batch.buffers) {
            if (!buffer) {
                return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    std::string(m_owner.begin(), m_owner.end()) + " replacement rejects null buffers"));
            }
        }
    }
    for (std::size_t index = 0; index < batches.size(); ++index) {
        if (auto replaced = m_batches[index].prepared.replace(
                batches[index].buffers); !replaced) {
            return replaced;
        }
    }
    return ::media::Status::success();
}

::media::Status MediaReservedOutputTransaction::authorize(
    std::span<const MediaOutputCapacityReservationHandle> handles,
    const Authorization& authorization, const MediaOutputReservationStoragePlan& storage)
{
    auto allocation = storage.allocation();
    if (auto valid = MediaOutputReservationStoragePlanner::validate(storage, allocation); !valid) return valid;
    if (handles.empty() || !authorization) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Reserved output authorization requires reservations and callback"));
    auto* resource = MediaLineageAllocator<std::byte>{allocation}.resource();
    if (!resource) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Reserved output authorization requires its scratch allocation owner"));
    MediaFixedCapacityStorage<std::shared_ptr<MediaOutputCapacityReservationRecord>> records(0, *resource);
    MediaFixedCapacityStorage<MediaChannel*> channels(0, *resource);
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> recordLocks(0, *resource);
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> channelLocks(0, *resource);
    try {
        records = MediaFixedCapacityStorage<std::shared_ptr<MediaOutputCapacityReservationRecord>>(handles.size(), *resource);
        for (const auto& handle : handles) {
            if (!handle.m_record) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Reserved output authorization rejects an invalid handle"));
            records.push_back(handle.m_record);
        }
        auto ordered = records.view();
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) { return lhs->identity < rhs->identity; });
        if (std::adjacent_find(ordered.begin(), ordered.end()) != ordered.end())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Reserved output authorization rejects duplicate handles"));
        std::uint64_t total = 0;
        for (const auto& record : records) {
            // Immutable after record construction; no state is inspected before locking.
            auto count = MediaCheckedArithmetic::add(total, record->channels.size(), "authorization channel scratch");
            if (!count) return ::media::Status::failure(count.error());
            total = count.value();
        }
        if (total > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Authorization scratch exceeds addressable size"));
        channels = MediaFixedCapacityStorage<MediaChannel*>(static_cast<std::size_t>(total), *resource);
        for (const auto& record : records)
            for (const auto& item : record->channels) channels.push_back(item.channel);
        auto orderedChannels = channels.view();
        std::sort(orderedChannels.begin(), orderedChannels.end(), [](MediaChannel* lhs, MediaChannel* rhs) {
            if (lhs->id() != rhs->id()) return lhs->id() < rhs->id();
            return std::less<MediaChannel*>{}(lhs, rhs);
        });
        MediaChannel* previous = nullptr;
        channels.eraseIf([&](MediaChannel* channel) {
            if (channel == previous) return true;
            previous = channel;
            return false;
        });
        if (storage.prepared()) {
            auto cost = MediaOutputReservationStoragePlanner::authorizationBytes(*storage.prepared(), records.size(), channels.capacity(), channels.size());
            if (!cost) return ::media::Status::failure(cost.error());
        }
        recordLocks = MediaFixedCapacityStorage<std::unique_lock<std::mutex>>(records.size(), *resource);
        channelLocks = MediaFixedCapacityStorage<std::unique_lock<std::mutex>>(channels.size(), *resource);
        for (std::size_t i = 0; i < records.size(); ++i) recordLocks.push_back(std::unique_lock<std::mutex>{});
        for (std::size_t i = 0; i < channels.size(); ++i) channelLocks.push_back(std::unique_lock<std::mutex>{});
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Reserved output authorization scratch allocation failed"));
    } catch (const std::length_error&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Reserved output authorization scratch size is not representable"));
    }
    for (std::size_t i = 0; i < records.size(); ++i) recordLocks[i] = std::unique_lock<std::mutex>(records[i]->mutex);
    for (const auto& record : records) {
        if (record->state != MediaOutputCapacityReservationState::Pending)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Reserved output authorization requires pending reservations"));
    }
    for (std::size_t i = 0; i < channels.size(); ++i)
        channelLocks[i] = std::unique_lock<std::mutex>(channels[i]->m_mutationMutex);
    for (const auto& record : records) {
        for (const auto& item : record->channels) {
            if (!item.channel->m_queue || item.channel->m_queue->aborted() ||
                item.channel->m_queue->closed() || item.channel->m_closeRequested ||
                item.channel->m_reservedCapacity < item.count)
                return ::media::Status::failure(::media::ErrorInfo::cancelled(
                    "Reserved output authorization found a closed output"));
        }
    }
    if (auto status = authorization(); !status) return status;
    for (const auto& record : records) {
        record->state = MediaOutputCapacityReservationState::Authorized;
        for (const auto& item : record->channels) item.channel->m_authorizedCapacity += item.count;
    }
    return ::media::Status::success();
}

::media::Status MediaReservedOutputTransaction::commit(
    const Authorization& finalAuthorization)
{
    if (!m_record) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            std::string(m_owner.begin(), m_owner.end()) + " reservation is not pending"));
    }
    std::unique_lock recordLock(m_record->mutex);
    if (m_record->state != MediaOutputCapacityReservationState::Authorized) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            std::string(m_owner.begin(), m_owner.end()) + " reservation is not authorized"));
    }
    MediaReservedOutputLockScope outputLocks(
        m_channelLocks.view(), m_queueLocks.view());
    try {
        for (std::size_t index = 0; index < m_channels.size(); ++index) {
            m_channelLocks[index] = std::unique_lock<std::mutex>(
                m_channels[index]->m_mutationMutex);
        }
        for (std::size_t index = 0; index < m_channels.size(); ++index) {
            auto* queue = static_cast<MediaBlockingQueue*>(
                m_channels[index]->m_queue.get());
            m_queueLocks[index] =
                std::unique_lock<std::mutex>(queue->m_mutex);
        }
        for (const auto& item : m_record->channels) {
            auto* queue = static_cast<MediaBlockingQueue*>(
                item.channel->m_queue.get());
            if (!queue->m_aborted) continue;
            for (const auto& release : m_record->channels) {
                release.channel->m_reservedCapacity -= release.count;
                release.channel->m_authorizedCapacity -= release.count;
            }
            m_record->state = MediaOutputCapacityReservationState::Cancelled;
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                std::string(m_owner.begin(), m_owner.end()) + " reservation was aborted"));
        }
        if (finalAuthorization) {
            if (auto authorized = finalAuthorization(); !authorized) {
                return authorized;
            }
        }
    } catch (const std::exception& exception) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                std::string(m_owner.begin(), m_owner.end()) + " commit threw: " + exception.what()));
    } catch (...) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                std::string(m_owner.begin(), m_owner.end()) + " commit threw"));
    }
    for (auto& batch : m_batches) {
        const std::size_t count = batch.prepared.size();
        batch.queue->publishPreparedLocked(batch.prepared);
        batch.channel->m_metrics.pushed.fetch_add(
            count, std::memory_order_relaxed);
        batch.channel->refreshQueueMetrics();
    }
    for (const auto& item : m_record->channels) {
        item.channel->m_reservedCapacity -= item.count;
        item.channel->m_authorizedCapacity -= item.count;
        auto* queue = static_cast<MediaBlockingQueue*>(
            item.channel->m_queue.get());
        if (item.channel->m_closeRequested &&
            item.channel->m_authorizedCapacity == 0) {
            queue->m_closed = true;
        }
    }
    m_record->state = MediaOutputCapacityReservationState::Committed;
    recordLock.unlock();
    outputLocks.unlock();
    for (MediaChannel* channel : m_channels) {
        auto* queue = static_cast<MediaBlockingQueue*>(
            channel->m_queue.get());
        queue->notifyPreparedPublished();
        if (channel->m_closeRequested) {
            queue->m_notEmpty.notify_all();
            queue->m_notFull.notify_all();
        }
        channel->publishAcceptedMutation();
    }
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
    MediaReservedOutputLockScope outputLocks(
        m_channelLocks.view(), m_queueLocks.view());
    for (std::size_t index = 0; index < m_channels.size(); ++index) {
        m_channelLocks[index] = std::unique_lock<std::mutex>(
            m_channels[index]->m_mutationMutex);
    }
    for (const auto& item : m_record->channels) {
        item.channel->m_reservedCapacity -= item.count;
        if (authorized) item.channel->m_authorizedCapacity -= item.count;
        item.channel->finalizeDeferredCloseLocked();
    }
    m_record->state = MediaOutputCapacityReservationState::Cancelled;
    recordLock.unlock();
    outputLocks.unlock();
    for (MediaChannel* channel : m_channels) {
        channel->publishReservedCapacityMutation();
    }
    m_record.reset();
    m_batches.clear();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
