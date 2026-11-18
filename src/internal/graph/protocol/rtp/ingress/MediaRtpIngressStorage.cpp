#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"

#include "internal/graph/runtime/resource/MediaPreparedAllocationCost.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isPowerOfTwo(std::size_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

::media::ErrorInfo invalidStorage(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(reason);
}

bool isKnownChannel(MediaRtpUdpChannel channel) noexcept
{
    switch (channel) {
    case MediaRtpUdpChannel::Rtp:
    case MediaRtpUdpChannel::Rtcp:
        return true;
    }
    return false;
}

} // namespace

MediaRtpIngressStorageState::MediaRtpIngressStorageState(MediaLineageAllocationScope scope)
    : allocation(std::move(scope)), entries(allocation.resource()),
      entrySlotIndices(allocation.resource()), slotOwnership(allocation.resource()) {}

MediaRtpIngressStorageState::~MediaRtpIngressStorageState()
{
    if (arena) {
        allocation.resource()->deallocate(arena, byteCapacity, alignmentBytes);
    }
}

void MediaRtpIngressStorageState::releaseBatch() noexcept
{
    std::lock_guard lock(leaseMutex);
    if (!leased) return;
    for (std::size_t index = 0; index < committedEntries; ++index) {
        const std::size_t slotIndex = entrySlotIndices[index];
        if (slotIndex < slotOwnership.size() &&
            slotOwnership[slotIndex] == SlotOwnership::BatchOwned) {
            slotOwnership[slotIndex] = SlotOwnership::Free;
            nextFreeSearch = (std::min)(nextFreeSearch, slotIndex);
        }
    }
    committedEntries = 0;
    leased = false;
    leaseReleased.notify_all();
}

MediaRtpIngressStorage::MediaRtpIngressStorage(
    std::shared_ptr<MediaRtpIngressStorageState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaRtpIngressStorage> MediaRtpIngressStorage::create(
    std::size_t byteCapacity,
    std::size_t maximumDatagramBytes,
    std::size_t descriptorCapacity,
    std::size_t alignmentBytes, MediaLineageAllocationScope allocation)
{
    if (!MediaLineageAllocator<std::byte>{allocation}.resource() ||
        byteCapacity == 0 || maximumDatagramBytes == 0 ||
        descriptorCapacity == 0 || !isPowerOfTwo(alignmentBytes) ||
        alignmentBytes < alignof(std::max_align_t) ||
        descriptorCapacity >
            (std::numeric_limits<std::size_t>::max)() /
                maximumDatagramBytes ||
        byteCapacity != descriptorCapacity * maximumDatagramBytes) {
        return ::media::Result<MediaRtpIngressStorage>::failure(
            invalidStorage(
                "RTP ingress storage requires exact bounded byte and descriptor geometry"));
    }
    try {
        auto state = std::allocate_shared<MediaRtpIngressStorageState>(
            MediaLineageAllocator<MediaRtpIngressStorageState>{allocation}, allocation);
        if (descriptorCapacity > state->entries.max_size()) {
            return ::media::Result<MediaRtpIngressStorage>::failure(
                invalidStorage(
                    "RTP ingress descriptor capacity exceeds the storage limit"));
        }
        state->arena = static_cast<std::byte*>(state->allocation.resource()->allocate(
            byteCapacity, alignmentBytes));
        state->byteCapacity = byteCapacity;
        state->maximumDatagramBytes = maximumDatagramBytes;
        state->descriptorCapacity = descriptorCapacity;
        state->alignmentBytes = alignmentBytes;
        state->entries.resize(descriptorCapacity,
            MediaRtpIngressBatchEntry{MediaRtpUdpChannel::Rtp, {}, 0});
        state->entrySlotIndices.resize(descriptorCapacity, 0);
        state->slotOwnership.resize(descriptorCapacity,
            MediaRtpIngressStorageState::SlotOwnership::Free);
        return ::media::Result<MediaRtpIngressStorage>::success(
            MediaRtpIngressStorage(std::move(state)));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaRtpIngressStorage>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP ingress storage allocation failed"));
    }
}

::media::Result<std::size_t> MediaRtpIngressStorage::preparationStorageBytes(
    std::size_t byteCapacity, std::size_t descriptorCapacity, std::size_t alignmentBytes)
{
    using Cost = MediaPreparedAllocationCost;
    return Cost::sum({Cost::shared<MediaRtpIngressStorageState>(),
        MediaPreparationMemoryResource::allocationFootprint(byteCapacity, alignmentBytes),
        Cost::fixedVector<MediaRtpIngressBatchEntry>(descriptorCapacity),
        Cost::fixedVector<std::size_t>(descriptorCapacity),
        Cost::fixedVector<MediaRtpIngressStorageState::SlotOwnership>(descriptorCapacity)});
}

std::byte* MediaRtpIngressStorage::data() noexcept
{
    return m_state ? m_state->arena : nullptr;
}

std::size_t MediaRtpIngressStorage::byteCapacity() const noexcept
{
    return m_state ? m_state->byteCapacity : 0;
}

std::size_t MediaRtpIngressStorage::maximumDatagramBytes() const noexcept
{
    return m_state ? m_state->maximumDatagramBytes : 0;
}

std::size_t MediaRtpIngressStorage::descriptorCapacity() const noexcept
{
    return m_state ? m_state->descriptorCapacity : 0;
}

std::size_t MediaRtpIngressStorage::committedEntries() const noexcept
{
    return m_state ? m_state->committedEntries : 0;
}

::media::Result<MediaRtpIngressWritableSlot>
MediaRtpIngressStorage::acquireReceiveSlot()
{
    std::unique_lock<std::mutex> stateLock;
    if (m_state) stateLock = std::unique_lock(m_state->leaseMutex);
    if (!m_state || !m_state->arena) {
        return ::media::Result<MediaRtpIngressWritableSlot>::failure(
            invalidStorage(
                "RTP ingress storage is unavailable"));
    }
    for (std::size_t offset = 0;
         offset < m_state->descriptorCapacity; ++offset) {
        const std::size_t slotIndex =
            (m_state->nextFreeSearch + offset) %
            m_state->descriptorCapacity;
        if (m_state->slotOwnership[slotIndex] !=
            MediaRtpIngressStorageState::SlotOwnership::Free) {
            continue;
        }
        m_state->slotOwnership[slotIndex] =
            MediaRtpIngressStorageState::SlotOwnership::ReceiveOwned;
        m_state->nextFreeSearch =
            (slotIndex + 1) % m_state->descriptorCapacity;
        const std::size_t byteOffset =
            slotIndex * m_state->maximumDatagramBytes;
        return ::media::Result<MediaRtpIngressWritableSlot>::success(
            MediaRtpIngressWritableSlot{
                slotIndex,
                byteOffset,
                std::span<std::byte>(
                    m_state->arena + byteOffset,
                    m_state->maximumDatagramBytes)});
    }
    return ::media::Result<MediaRtpIngressWritableSlot>::failure(
        ::media::ErrorInfo::wouldBlock(
            "RTP ingress storage has no free receive slot"));
}

::media::Status MediaRtpIngressStorage::releaseReceiveSlot(
    std::size_t slotIndex)
{
    std::unique_lock<std::mutex> stateLock;
    if (m_state) stateLock = std::unique_lock(m_state->leaseMutex);
    if (!m_state || slotIndex >= m_state->descriptorCapacity ||
        m_state->slotOwnership[slotIndex] !=
            MediaRtpIngressStorageState::SlotOwnership::ReceiveOwned) {
        return ::media::Status::failure(invalidStorage(
            "RTP ingress receive slot is not owned by the adapter"));
    }
    m_state->slotOwnership[slotIndex] =
        MediaRtpIngressStorageState::SlotOwnership::Free;
    m_state->nextFreeSearch = (std::min)(
        m_state->nextFreeSearch, slotIndex);
    return ::media::Status::success();
}

::media::Status MediaRtpIngressStorage::commitReceivedSlot(
    std::size_t slotIndex,
    MediaRtpUdpChannel channel,
    std::size_t byteCount,
    std::int64_t observedAtNanoseconds)
{
    std::unique_lock<std::mutex> stateLock;
    if (m_state) stateLock = std::unique_lock(m_state->leaseMutex);
    if (!m_state || m_state->leased ||
        slotIndex >= m_state->descriptorCapacity ||
        m_state->committedEntries >= m_state->descriptorCapacity ||
        m_state->slotOwnership[slotIndex] !=
            MediaRtpIngressStorageState::SlotOwnership::ReceiveOwned ||
        byteCount == 0 ||
        byteCount > m_state->maximumDatagramBytes || !isKnownChannel(channel) ||
        observedAtNanoseconds <= 0 ||
        observedAtNanoseconds <
            m_state->lastCommittedObservationNanoseconds) {
        return ::media::Status::failure(invalidStorage(
            "RTP ingress completion is invalid, regressing, or out of bounds"));
    }
    const std::size_t entryIndex = m_state->committedEntries;
    m_state->entries[entryIndex] = MediaRtpIngressBatchEntry{
        channel,
        std::span<const std::byte>(
            m_state->arena + slotIndex * m_state->maximumDatagramBytes,
            byteCount),
        observedAtNanoseconds};
    m_state->entrySlotIndices[entryIndex] = slotIndex;
    m_state->slotOwnership[slotIndex] =
        MediaRtpIngressStorageState::SlotOwnership::BatchOwned;
    m_state->lastCommittedObservationNanoseconds =
        observedAtNanoseconds;
    ++m_state->committedEntries;
    return ::media::Status::success();
}

::media::Result<MediaRtpIngressBatch>
MediaRtpIngressStorage::seal(std::size_t entryCount)
{
    std::unique_lock<std::mutex> stateLock;
    if (m_state) stateLock = std::unique_lock(m_state->leaseMutex);
    if (!m_state || m_state->leased || entryCount == 0 ||
        entryCount != m_state->committedEntries) {
        return ::media::Result<MediaRtpIngressBatch>::failure(
            invalidStorage(
                "RTP ingress batch seal requires every committed descriptor exactly once"));
    }
    m_state->leased = true;
    return ::media::Result<MediaRtpIngressBatch>::success(
        MediaRtpIngressBatch(
            m_state,
            std::span<const MediaRtpIngressBatchEntry>(
                m_state->entries.data(), entryCount)));
}

::media::Status MediaRtpIngressStorage::waitForBatchRelease(std::stop_token stopToken)
{
    if (!m_state) return ::media::Status::failure(invalidStorage("RTP ingress storage is unavailable"));
    std::stop_callback cancelled(stopToken, [state = m_state] {
        std::lock_guard lock(state->leaseMutex);
        state->leaseReleased.notify_all();
    });
    std::unique_lock lock(m_state->leaseMutex);
    m_state->leaseReleased.wait(lock, [&] { return !m_state->leased || stopToken.stop_requested(); });
    if (stopToken.stop_requested()) return ::media::Status::failure(
        ::media::ErrorInfo::cancelled("RTP ingress batch reuse was cancelled"));
    return ::media::Status::success();
}

::media::Status MediaRtpIngressStorage::reset()
{
    std::unique_lock<std::mutex> stateLock;
    if (m_state) stateLock = std::unique_lock(m_state->leaseMutex);
    if (!m_state || !m_state->arena || m_state->leased) {
        return ::media::Status::failure(invalidStorage(
            "RTP ingress storage cannot reset while a batch lease is active"));
    }
    for (std::size_t index = 0;
         index < m_state->committedEntries; ++index) {
        const std::size_t slotIndex = m_state->entrySlotIndices[index];
        if (slotIndex < m_state->slotOwnership.size() &&
            m_state->slotOwnership[slotIndex] ==
                MediaRtpIngressStorageState::SlotOwnership::BatchOwned) {
            m_state->slotOwnership[slotIndex] =
                MediaRtpIngressStorageState::SlotOwnership::Free;
            m_state->nextFreeSearch = (std::min)(
                m_state->nextFreeSearch, slotIndex);
        }
    }
    m_state->committedEntries = 0;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
