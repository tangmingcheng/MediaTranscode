#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <initializer_list>
#include <limits>

namespace media::ffmpeg::graph {

// Costs use the same allocator types as the prepared owners. They include
// allocation headers/alignment and audited STL proxies, not heap internals.
struct MediaPreparedAllocationCost final {
    template<class T> static ::media::Result<std::size_t> shared()
    {
        auto geometry = probeMediaSharedAllocationGeometry<T>(
            std::type_identity<MediaLineageAllocator<T>>{});
        if (!geometry) return ::media::Result<std::size_t>::failure(geometry.error());
        return MediaPreparationMemoryResource::allocationFootprint(
            geometry.value().bytes, geometry.value().alignment);
    }
    template<class T> static ::media::Result<std::size_t> object()
    { return MediaPreparationMemoryResource::allocationFootprint(sizeof(T), alignof(T)); }
    template<class T> static ::media::Result<std::size_t> fixedVector(std::size_t count)
    {
        auto geometry = MediaPmrContainerGeometry::vector<T>(count, false);
        return geometry ? ::media::Result<std::size_t>::success(geometry.value().peakBytes)
                        : ::media::Result<std::size_t>::failure(geometry.error());
    }
    static ::media::Result<std::size_t> sum(
        std::initializer_list<::media::Result<std::size_t>> parts)
    {
        std::uint64_t total = 0;
        for (const auto& part : parts) {
            if (!part) return ::media::Result<std::size_t>::failure(part.error());
            auto next = MediaCheckedArithmetic::add(total, part.value(), "Prepared allocation storage");
            if (!next) return ::media::Result<std::size_t>::failure(next.error());
            total = next.value();
        }
        if (total > (std::numeric_limits<std::size_t>::max)())
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::allocationFailed("Prepared allocation exceeds addressable storage"));
        return ::media::Result<std::size_t>::success(static_cast<std::size_t>(total));
    }
};
} // namespace media::ffmpeg::graph
