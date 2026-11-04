#pragma once
#include "internal/graph/model/MediaAvStartupGenerationStoragePlan.h"
#include <memory_resource>

namespace media::ffmpeg::graph {
// MSVC hash lifecycle: head, initial buckets, optional empty-table rehash,
// then node allocations only. Bucket identity never depends on request size.
class MediaAvStartupHashMemoryResource final : public std::pmr::memory_resource {
public:
    MediaAvStartupHashMemoryResource(const MediaAvStartupGenerationStoragePlan& plan,
        std::size_t hashIndex, std::pmr::memory_resource& upstream) noexcept;
    ~MediaAvStartupHashMemoryResource() override;
    MediaAvStartupHashMemoryResource(const MediaAvStartupHashMemoryResource&) = delete;
    MediaAvStartupHashMemoryResource& operator=(const MediaAvStartupHashMemoryResource&) = delete;
    ::media::Status beginRehash();
    ::media::Status seal(std::size_t bucketCount, float maximumLoadFactor);
private:
    enum class Phase { Head, InitialBuckets, Ready, Rehash, Sealed };
    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;
    std::pmr::memory_resource& m_upstream;
    const MediaAvStartupHashStoragePlan* m_prepared;
    Phase m_phase = Phase::Head;
    std::size_t m_liveNodes = 0;
    void* m_initialBuckets = nullptr;
    void* m_finalBuckets = nullptr;
};
} // namespace media::ffmpeg::graph
