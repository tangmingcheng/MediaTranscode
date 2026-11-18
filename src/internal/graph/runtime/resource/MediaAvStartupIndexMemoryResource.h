#pragma once
#include "internal/graph/model/MediaAvStartupIndexStoragePlan.h"
#include <memory_resource>

namespace media::ffmpeg::graph {

// Single coordinator thread; counters cover outstanding allocator requests.
// The owning Store retains its scope before this resource and its containers.
class MediaAvStartupIndexMemoryResource final : public std::pmr::memory_resource {
public:
    MediaAvStartupIndexMemoryResource(const MediaAvStartupIndexStoragePlan& plan,
        MediaAvStartupIndexClass kind, std::pmr::memory_resource& upstream) noexcept;
    ~MediaAvStartupIndexMemoryResource() override;
    MediaAvStartupIndexMemoryResource(const MediaAvStartupIndexMemoryResource&) = delete;
    MediaAvStartupIndexMemoryResource& operator=(const MediaAvStartupIndexMemoryResource&) = delete;
private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;
    std::pmr::memory_resource& m_upstream;
    const MediaAvStartupIndexAllocationPlan* m_prepared;
    std::size_t m_live = 0;
};
} // namespace media::ffmpeg::graph
