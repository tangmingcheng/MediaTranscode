#include "internal/graph/runtime/resource/MediaAvStartupHashMemoryResource.h"
#include <cassert>
#include <exception>
#include <new>

namespace media::ffmpeg::graph {
MediaAvStartupHashMemoryResource::MediaAvStartupHashMemoryResource(
    const MediaAvStartupGenerationStoragePlan& plan, std::size_t hashIndex,
    std::pmr::memory_resource& upstream) noexcept : m_upstream(upstream), m_prepared(nullptr), m_phase(Phase::Ready)
{
    if (const auto* prepared = std::get_if<MediaPreparedStartupGenerationStorage>(&plan.storage))
        m_prepared = &prepared->hashes[hashIndex];
    if (m_prepared) {
        const auto& geometry = m_prepared->geometry;
        m_phase = geometry.initialNodeRequests ? Phase::Head
            : geometry.initialBuckets ? Phase::InitialBuckets : Phase::Ready;
    }
}
MediaAvStartupHashMemoryResource::~MediaAvStartupHashMemoryResource()
{ assert(m_liveNodes == 0 && !m_initialBuckets && !m_finalBuckets); }
::media::Status MediaAvStartupHashMemoryResource::beginRehash()
{
    if (!m_prepared || m_phase != Phase::Ready ||
        m_liveNodes != m_prepared->geometry.initialNodeRequests)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup hash has no prepared empty-table rehash phase"));
    m_phase = Phase::Rehash;
    return ::media::Status::success();
}
::media::Status MediaAvStartupHashMemoryResource::seal(std::size_t bucketCount, float maximumLoadFactor)
{
    if (!m_prepared || m_phase != Phase::Rehash ||
        bucketCount != m_prepared->geometry.bucketCount ||
        maximumLoadFactor != m_prepared->geometry.maximumLoadFactor ||
        m_liveNodes != m_prepared->geometry.initialNodeRequests ||
        (m_prepared->geometry.initialBuckets && bucketCount == m_prepared->geometry.initialBucketCount
            ? (!m_initialBuckets || m_finalBuckets) : (m_initialBuckets || !m_finalBuckets)))
        return ::media::Status::failure(::media::ErrorInfo::unsupported("Startup hash rehash readback differs from its prepared geometry"));
    m_phase = Phase::Sealed;
    return ::media::Status::success();
}
void* MediaAvStartupHashMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment)
{
    if (!m_prepared) return m_upstream.allocate(bytes, alignment);
    const auto request = MediaAvStartupIndexGeometry{bytes, alignment};
    if (m_phase == Phase::Head || m_phase == Phase::Sealed) {
        if (request != m_prepared->geometry.node || m_liveNodes >= m_prepared->nodes.maximumLiveRequests)
            throw std::bad_alloc();
        auto* pointer = m_upstream.allocate(bytes, alignment);
        ++m_liveNodes;
        if (m_phase == Phase::Head && m_liveNodes == m_prepared->geometry.initialNodeRequests)
            m_phase = m_prepared->geometry.initialBuckets ? Phase::InitialBuckets : Phase::Ready;
        return pointer;
    }
    if (m_phase == Phase::InitialBuckets) {
        if (m_initialBuckets || !m_prepared->geometry.initialBuckets ||
            request != *m_prepared->geometry.initialBuckets) throw std::bad_alloc();
        m_initialBuckets = m_upstream.allocate(bytes, alignment);
        m_phase = Phase::Ready;
        return m_initialBuckets;
    }
    if (m_phase == Phase::Rehash && !m_finalBuckets &&
        (m_prepared->geometry.initialBuckets.has_value() == (m_initialBuckets != nullptr)) &&
        (!m_prepared->geometry.initialBuckets ||
            m_prepared->geometry.bucketCount != m_prepared->geometry.initialBucketCount) &&
        request == m_prepared->geometry.finalBuckets) {
        m_finalBuckets = m_upstream.allocate(bytes, alignment);
        return m_finalBuckets;
    }
    throw std::bad_alloc();
}
void MediaAvStartupHashMemoryResource::do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment)
{
    if (!m_prepared) { m_upstream.deallocate(pointer, bytes, alignment); return; }
    const auto request = MediaAvStartupIndexGeometry{bytes, alignment};
    if (pointer == m_initialBuckets || pointer == m_finalBuckets) {
        auto& slot = pointer == m_initialBuckets ? m_initialBuckets : m_finalBuckets;
        if (pointer == m_initialBuckets && !m_prepared->geometry.initialBuckets) std::terminate();
        const auto expected = pointer == m_initialBuckets
            ? *m_prepared->geometry.initialBuckets : m_prepared->geometry.finalBuckets;
        if (request != expected) std::terminate();
        m_upstream.deallocate(pointer, bytes, alignment);
        slot = nullptr;
    } else {
        if (!m_liveNodes || request != m_prepared->geometry.node) std::terminate();
        m_upstream.deallocate(pointer, bytes, alignment);
        --m_liveNodes;
    }
}
bool MediaAvStartupHashMemoryResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{ return this == &other; }
} // namespace media::ffmpeg::graph
