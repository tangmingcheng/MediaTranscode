#include "internal/graph/runtime/resource/MediaRtpProtocolStorageRegistry.h"
#include <cassert>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
MediaRtpProtocolStoragePermit::MediaRtpProtocolStoragePermit(
    std::shared_ptr<MediaRtpProtocolStorageRegistry> owner,
    std::size_t index, std::size_t capacity) noexcept
    : m_owner(std::move(owner)), m_index(index), m_capacity(capacity) {}
MediaRtpProtocolStoragePermit::MediaRtpProtocolStoragePermit(
    MediaRtpProtocolStoragePermit&& other) noexcept
    : m_owner(std::move(other.m_owner)), m_index(other.m_index), m_capacity(other.m_capacity) {}
MediaRtpProtocolStoragePermit::~MediaRtpProtocolStoragePermit()
{
    if (m_owner) m_owner->release(m_index);
}
std::pmr::memory_resource& MediaRtpProtocolStoragePermit::resource() const noexcept
{
    assert(m_owner);
    return m_owner->m_entries[m_index].recordResource;
}
const MediaSharedAllocationGeometry& MediaRtpProtocolStoragePermit::allocationGeometry() const noexcept
{
    assert(m_owner);
    return m_owner->m_entries[m_index].plan.recordAllocation;
}
MediaRtpProtocolStorageRegistry::Entry::Entry(
    MediaRtpProtocolStoragePlan planned, MediaOwnedMemoryAllocation allocation) noexcept
    : plan(planned), backing(std::move(allocation)),
      recordResource(backing.data(), backing.bytes(), std::pmr::null_memory_resource())
{
}
MediaRtpProtocolStorageRegistry::Entry::Entry(Entry&& other) noexcept
    : Entry(other.plan, std::move(other.backing))
{
    // Entries move only while constructing the unpublished fixed table.
    if (other.claimed) std::terminate();
}
MediaRtpProtocolStorageRegistry::MediaRtpProtocolStorageRegistry(
    std::span<const MediaRtpProtocolStoragePlan> plans,
    std::shared_ptr<MediaNetworkMemoryResource> resource)
    : m_resource(std::move(resource)), m_entries(plans.size(), *m_resource)
{
}
::media::Result<std::size_t> MediaRtpProtocolStorageRegistry::storageFootprint(
    std::uint64_t instances)
{
    if (instances == 0) return ::media::Result<std::size_t>::success(0);
    if (instances > (std::numeric_limits<std::size_t>::max)() / sizeof(Entry))
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::invalidArgument(
            "RTP instance table is not addressable"));
    return MediaNetworkMemoryResource::allocationFootprint(
        static_cast<std::size_t>(instances) * sizeof(Entry), alignof(Entry));
}
::media::Result<std::shared_ptr<MediaRtpProtocolStorageRegistry>>
MediaRtpProtocolStorageRegistry::create(std::span<const MediaRtpProtocolStoragePlan> plans,
    std::shared_ptr<MediaNetworkMemoryResource> resource)
{
    using Result = ::media::Result<std::shared_ptr<MediaRtpProtocolStorageRegistry>>;
    auto footprint = storageFootprint(plans.size());
    if (!footprint) return Result::failure(footprint.error());
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP instance registry requires network storage"));
    std::size_t recordBytes = 0;
    for (std::size_t i = 0; i < plans.size(); ++i) {
        const auto& p = plans[i];
        if (!p.rtpEndpointId || !p.rtcpEndpointId || p.rtpEndpointId == p.rtcpEndpointId ||
            !p.recordCapacity || p.recordCapacity > (std::numeric_limits<std::size_t>::max)() ||
            p.recordAllocation.count != p.recordCapacity || !p.recordAllocation.bytes ||
            p.recordAllocation.bytes % p.recordCapacity != 0)
            return Result::failure(::media::ErrorInfo::invalidArgument("Invalid RTP storage instance"));
        auto cost = MediaNetworkMemoryResource::allocationFootprint(
            p.recordAllocation.bytes, p.recordAllocation.alignment);
        if (!cost) return Result::failure(cost.error());
        if (cost.value() > (std::numeric_limits<std::size_t>::max)() - recordBytes)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP record backing sum overflows"));
        recordBytes += cost.value();
        for (std::size_t j = 0; j < i; ++j) {
            const auto& q = plans[j];
            if (p.rtpEndpointId == q.rtpEndpointId || p.rtpEndpointId == q.rtcpEndpointId ||
                p.rtcpEndpointId == q.rtpEndpointId || p.rtcpEndpointId == q.rtcpEndpointId)
                return Result::failure(::media::ErrorInfo::invalidArgument("RTP instance endpoints overlap"));
        }
    }
    try {
        auto registry = std::shared_ptr<MediaRtpProtocolStorageRegistry>(
            new MediaRtpProtocolStorageRegistry(plans, resource));
        if (plans.empty()) return Result::success(std::move(registry));
        auto backing = resource->reserveStorage(recordBytes, MediaStorageAdmission::RequiredAllocation);
        if (!backing) return Result::failure(backing.error());
        for (const auto& plan : plans) {
            const auto& geometry = plan.recordAllocation;
            auto cost = MediaNetworkMemoryResource::allocationFootprint(geometry.bytes, geometry.alignment);
            if (!cost) return Result::failure(cost.error());
            auto portion = backing.value().split(cost.value());
            if (!portion) return Result::failure(portion.error());
            auto records = resource->allocateFromReservation(
                geometry.bytes, geometry.alignment, std::move(portion).value());
            if (!records) return Result::failure(records.error());
            registry->m_entries.push_back(Entry(plan, std::move(records).value()));
        }
        return Result::success(std::move(registry));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("RTP storage registry"));
    }
}
::media::Result<MediaRtpProtocolStoragePermit> MediaRtpProtocolStorageRegistry::claim(
    std::uint64_t rtpEndpointId, std::uint64_t rtcpEndpointId,
    std::size_t capacity, std::shared_ptr<MediaNodeWakeup> wakeup)
{
    using Result = ::media::Result<MediaRtpProtocolStoragePermit>;
    if (!wakeup) return Result::failure(::media::ErrorInfo::invalidArgument("RTP instance wakeup is missing"));
    auto owner = weak_from_this().lock();
    if (!owner) return Result::failure(::media::ErrorInfo::notInitialized("RTP instance registry is not owned"));
    std::lock_guard lock(m_mutex);
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];
        if (entry.plan.rtpEndpointId != rtpEndpointId || entry.plan.rtcpEndpointId != rtcpEndpointId) continue;
        if (entry.claimed || entry.plan.recordCapacity != capacity)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP instance is active or has conflicting capacity"));
        entry.claimed = true;
        entry.wakeup = std::move(wakeup);
        return Result::success(MediaRtpProtocolStoragePermit(std::move(owner), i, capacity));
    }
    return Result::failure(::media::ErrorInfo::invalidArgument("RTP instance is outside the planned topology"));
}
void MediaRtpProtocolStorageRegistry::release(std::size_t index) noexcept
{
    std::lock_guard lock(m_mutex);
    assert(index < m_entries.size() && m_entries[index].claimed);
    // The protocol state's record objects have already been destroyed. Rebind
    // the arena to its original block: release() does not rewind the caller's
    // initial buffer on the supported MSVC STL implementation.
    auto& entry = m_entries[index];
    std::destroy_at(&entry.recordResource);
    std::construct_at(&entry.recordResource, entry.backing.data(),
        entry.backing.bytes(), std::pmr::null_memory_resource());
    m_entries[index].claimed = false;
    m_entries[index].wakeup.reset();
}
void MediaRtpProtocolStorageRegistry::notify() noexcept
{
    std::lock_guard lock(m_mutex);
    for (const auto& entry : m_entries)
        if (auto wakeup = entry.wakeup.lock()) wakeup->notify();
}
} // namespace media::ffmpeg::graph
