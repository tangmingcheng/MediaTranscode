#include "internal/graph/runtime/resource/MediaAvStartupIndexMemoryResource.h"
#include <cassert>
#include <exception>
#include <new>

namespace media::ffmpeg::graph {
MediaAvStartupIndexMemoryResource::MediaAvStartupIndexMemoryResource(
    const MediaAvStartupIndexStoragePlan& plan, MediaAvStartupIndexClass kind,
    std::pmr::memory_resource& upstream) noexcept
    : m_upstream(upstream), m_prepared(nullptr)
{
    if (const auto* prepared = std::get_if<MediaPreparedStartupIndexStorage>(&plan.storage))
        m_prepared = &prepared->allocations[static_cast<std::size_t>(kind)];
}
MediaAvStartupIndexMemoryResource::~MediaAvStartupIndexMemoryResource()
{ assert(m_live == 0); }
void* MediaAvStartupIndexMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment)
{
    if (m_prepared && (m_prepared->request != MediaAvStartupIndexGeometry{bytes, alignment} ||
        m_live >= m_prepared->maximumLiveRequests)) throw std::bad_alloc();
    // The original resource admits and charges exactly once, including its header.
    auto* result = m_upstream.allocate(bytes, alignment);
    if (m_prepared) ++m_live;
    return result;
}
void MediaAvStartupIndexMemoryResource::do_deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment)
{
    if (m_prepared) {
        if (!m_live || m_prepared->request != MediaAvStartupIndexGeometry{bytes, alignment})
            std::terminate();
    }
    m_upstream.deallocate(pointer, bytes, alignment);
    if (m_prepared) --m_live;
}
bool MediaAvStartupIndexMemoryResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{ return this == &other; }
} // namespace media::ffmpeg::graph
