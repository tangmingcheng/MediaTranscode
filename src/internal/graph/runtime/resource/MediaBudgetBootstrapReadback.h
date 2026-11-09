#pragma once
#include "internal/graph/runtime/resource/MediaBudgetBootstrapAllocator.h"
#include <memory_resource>
#include <utility>

namespace media::ffmpeg::graph {

// Observe the complete allocate_shared request, including the real allocator's
// rebind type. Native probe backing is temporary; the reported charge includes
// the same budget header/alignment used by the production bootstrap allocator.
template<class Budget, class Lease>
class MediaBudgetBootstrapReadback final {
    using Block = MediaBudgetAllocationBlock<std::shared_ptr<Budget>, Lease>;
    class Request final : public std::pmr::memory_resource {
    public:
        std::size_t bytes = 0;
    private:
        void* do_allocate(std::size_t requested, std::size_t alignment) override
        {
            if (bytes || !requested) throw std::bad_alloc();
            auto cost = allocationFootprint(requested, alignment);
            if (!cost) throw MediaBudgetBootstrapFailure{cost.error()};
            auto* pointer = std::pmr::new_delete_resource()->allocate(requested, alignment);
            bytes = cost.value();
            return pointer;
        }
        void do_deallocate(void* pointer, std::size_t requested, std::size_t alignment) override
        { std::pmr::new_delete_resource()->deallocate(pointer, requested, alignment); }
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        { return this == &other; }
    };
public:
    static ::media::Result<std::size_t> allocationFootprint(std::size_t bytes, std::size_t alignment)
    {
        auto layout = Block::layout(bytes, alignment);
        if (!layout) return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::allocationFailed("Budget bootstrap layout is invalid or overflows"));
        return ::media::Result<std::size_t>::success(layout->bytes);
    }
    template<class Factory>
    static ::media::Result<std::size_t> measure(Factory&& factory)
    {
        using R = ::media::Result<std::size_t>;
        try {
            Request readback;
            { auto owner = std::forward<Factory>(factory)(readback); }
            if (!readback.bytes) return R::failure(::media::ErrorInfo::unsupported(
                "Budget bootstrap requires an observed allocator request"));
            return R::success(readback.bytes);
        } catch (const MediaBudgetBootstrapFailure& failure) {
            return R::failure(failure.error);
        } catch (const std::bad_alloc&) {
            return R::failure(::media::ErrorInfo::allocationFailed(
                "Budget bootstrap requires a verified single allocation"));
        }
    }
};

} // namespace media::ffmpeg::graph
