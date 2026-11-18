#pragma once

#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaPmrContainerStorage final {
    std::size_t maximumElements = 0;
    std::size_t retainedBytes = 0;
    std::size_t peakBytes = 0;
};

// Vendor adapter for allocator requests, not heap implementation overhead.
// MSVC 145/202604: deque::_Growmap/_Emplace_back_internal and
// vector::_Calculate_growth/_Emplace_reallocate. GCC 12.4: vector::_M_check_len
// and deque::_M_initialize_map/_M_reallocate_map. No trial allocations.
template<class AllocationCharge>
class MediaContainerGeometry final {
public:
    template<class T>
    static ::media::Result<MediaPmrContainerStorage> rangeVector(std::size_t count)
    {
        using R = ::media::Result<MediaPmrContainerStorage>;
        auto proxy = proxyBytes();
        if (!proxy) return R::failure(proxy.error());
        if (!count) return R::success({0, proxy.value(), proxy.value()});
        const auto maximum = maximumElements<T>();
        if (count > maximum) return R::failure(::media::ErrorInfo::allocationFailed("Vector range exceeds max_size"));
        // A reallocating range insert has old size <= old capacity < count.
        // The vendor growth term is bounded by the largest such old size.
        auto grown = MediaCheckedArithmetic::add(count - 1,
            auditedGcc ? count - 1 : (count - 1) / 2, "Vector range growth");
        if (!grown || grown.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Vector range growth overflow"));
        const auto capacity = (std::min)(maximum, (std::max)(count, static_cast<std::size_t>(grown.value())));
        auto old = request<T>(count - 1);
        auto current = request<T>(capacity);
        if (!old) return R::failure(old.error());
        if (!current) return R::failure(current.error());
        auto retained = total(0, 0, proxy.value(), current.value(), 0);
        auto peak = total(0, 0, proxy.value(), current.value(), old.value());
        if (!retained) return R::failure(retained.error());
        if (!peak) return R::failure(peak.error());
        return R::success({count, retained.value(), peak.value()});
    }

    template<class Value>
    static ::media::Result<std::size_t> tree(std::size_t count)
    {
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
        using Node = std::_Tree_node<Value, void*>;
        auto node = request<Node>(1);
        auto proxy = proxyBytes();
        if (!node) return ::media::Result<std::size_t>::failure(node.error());
        if (!proxy) return ::media::Result<std::size_t>::failure(proxy.error());
        // Every live entry and the permanent head sentinel has its own request.
        return total(count, node.value(), proxy.value(), node.value(), 0);
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
        // GCC 12.4 _Rb_tree::_M_get_node allocates one _Rb_tree_node per
        // entry. The empty header is embedded in _Rb_tree_impl, not allocated.
        // Charge each node separately, including each preparation block header.
        using Node = std::_Rb_tree_node<Value>;
        auto node = request<Node>(1);
        if (!node) return ::media::Result<std::size_t>::failure(node.error());
        return total(count, node.value(), 0, 0, 0);
#else
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::unsupported("Tree geometry requires audited STL"));
#endif
    }
    template<class T>
    static ::media::Result<MediaPmrContainerStorage> deque(std::size_t count, bool rolling)
    {
        using R = ::media::Result<MediaPmrContainerStorage>;
        auto proxy = proxyBytes();
        if (!proxy) return R::failure(proxy.error());
        const auto maximum = (std::min)(
            static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()),
            (std::numeric_limits<std::size_t>::max)() / sizeof(T));
        if (count > maximum)
            return R::failure(::media::ErrorInfo::allocationFailed("PMR deque exceeds max_size"));
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
        // libstdc++ releases exhausted blocks on pop and recentres the live map.
        // The envelope permits every start offset, including after clear/pop.
        (void)rolling;
        const auto blockElements = std::__deque_buf_size(sizeof(T));
        const auto nodes = count / blockElements + (count % blockElements != 0) + 1;
        auto block = request<T>(blockElements);
        if (!block) return R::failure(block.error());
        auto threshold = MediaCheckedArithmetic::multiply(nodes, 2, "GCC deque recenter threshold");
        if (!threshold) return R::failure(threshold.error());
        std::size_t map = 8; // _S_initial_map_size in the audited vendor.
        std::size_t previous = 0;
        while (count && map <= threshold.value()) {
            auto doubled = MediaCheckedArithmetic::multiply(map, 2, "GCC deque map growth");
            if (!doubled) return R::failure(doubled.error());
            auto next = MediaCheckedArithmetic::add(doubled.value(), 2, "GCC deque map guard slots");
            if (!next || next.value() > (std::numeric_limits<std::size_t>::max)())
                return R::failure(::media::ErrorInfo::allocationFailed("GCC deque map size overflows"));
            previous = map;
            map = static_cast<std::size_t>(next.value());
        }
        auto mapBytes = request<T*>(map);
        auto oldMapBytes = request<T*>(previous);
        if (!mapBytes) return R::failure(mapBytes.error());
        if (!oldMapBytes) return R::failure(oldMapBytes.error());
        auto retained = total(nodes, block.value(), 0, mapBytes.value(), 0);
        auto peak = total(nodes, block.value(), 0, mapBytes.value(), oldMapBytes.value());
        if (!retained) return R::failure(retained.error());
        if (!peak) return R::failure(peak.error());
        return R::success({count, retained.value(), peak.value()});
#else
        // All current consumers have one-element blocks. Reject a new element
        // layout rather than silently applying this recurrence to another one.
        if constexpr (sizeof(T) <= 8)
            return R::failure(::media::ErrorInfo::unsupported("Deque geometry requires one-element blocks"));
        MediaPmrContainerStorage result{count, proxy.value(), proxy.value()};
        if (!count) return R::success(result);
        auto block = request<T>(1);
        if (!block) return R::failure(block.error());
        std::size_t map = 8;
        std::size_t previous = 0;
        while (true) {
            auto mapBytes = request<T*>(map);
            auto oldMapBytes = request<T*>(previous);
            if (!mapBytes) return R::failure(mapBytes.error());
            if (!oldMapBytes) return R::failure(oldMapBytes.error());
            const auto oldBlocks = previous ? (rolling ? previous : previous - 1) : 0;
            auto growth = total(oldBlocks, block.value(), proxy.value(), mapBytes.value(), oldMapBytes.value());
            if (!growth) return R::failure(growth.error());
            result.peakBytes = (std::max)(result.peakBytes, growth.value());
            if (map > count) {
                const auto blocks = rolling && count > 1 ? map : count;
                auto retained = total(blocks, block.value(), proxy.value(), mapBytes.value(), 0);
                if (!retained) return R::failure(retained.error());
                result.retainedBytes = retained.value();
                result.peakBytes = (std::max)(result.peakBytes, result.retainedBytes);
                return R::success(result);
            }
            auto next = MediaCheckedArithmetic::multiply(map, 2, "PMR deque map growth");
            if (!next || next.value() > (std::numeric_limits<std::size_t>::max)())
                return R::failure(::media::ErrorInfo::allocationFailed("PMR deque map growth overflow"));
            previous = map;
            map = static_cast<std::size_t>(next.value());
        }
#endif
    }

    template<class T>
    static ::media::Result<MediaPmrContainerStorage> vector(std::size_t count, bool incremental)
    {
        using R = ::media::Result<MediaPmrContainerStorage>;
        auto proxy = proxyBytes();
        if (!proxy) return R::failure(proxy.error());
        const auto maximum = maximumElements<T>();
        if (count > maximum)
            return R::failure(::media::ErrorInfo::allocationFailed("PMR vector exceeds max_size"));
        MediaPmrContainerStorage result{count, proxy.value(), proxy.value()};
        std::size_t capacity = 0;
        while (capacity < count) {
            auto geometric = MediaCheckedArithmetic::add(capacity, auditedGcc ? capacity : capacity / 2, "PMR vector growth");
            auto oneMore = MediaCheckedArithmetic::add(capacity, 1, "PMR vector growth");
            if (!geometric || !oneMore)
                return R::failure(::media::ErrorInfo::allocationFailed("PMR vector growth overflow"));
            const auto next = incremental ? (std::min)(static_cast<std::uint64_t>(maximum),
                (std::max)(geometric.value(), oneMore.value())) : count;
            if (next > (std::numeric_limits<std::size_t>::max)())
                return R::failure(::media::ErrorInfo::allocationFailed("PMR vector capacity overflow"));
            auto oldBytes = request<T>(capacity);
            auto newBytes = request<T>(static_cast<std::size_t>(next));
            if (!oldBytes) return R::failure(oldBytes.error());
            if (!newBytes) return R::failure(newBytes.error());
            auto peak = total(0, 0, proxy.value(), oldBytes.value(), newBytes.value());
            auto retained = total(0, 0, proxy.value(), newBytes.value(), 0);
            if (!peak) return R::failure(peak.error());
            if (!retained) return R::failure(retained.error());
            result.peakBytes = (std::max)(result.peakBytes, peak.value());
            result.retainedBytes = retained.value();
            capacity = static_cast<std::size_t>(next);
        }
        return R::success(result);
    }

    // A moved-from MSVC container still owns a proxy when iterator debugging
    // is enabled. Callers describe simultaneously live container objects.
    static ::media::Result<std::size_t> proxyBytes()
    {
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
        return ::media::Result<std::size_t>::success(0);
#elif !defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION != 145 || _MSVC_STL_UPDATE != 202604L
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::unsupported(
            "PMR container geometry requires an audited standard-library capability"));
#elif _ITERATOR_DEBUG_LEVEL == 0
        return ::media::Result<std::size_t>::success(0);
#else
        return request<std::_Container_proxy>(1);
#endif
    }

private:
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    static constexpr bool auditedGcc = true;
#else
    static constexpr bool auditedGcc = false;
#endif
    template<class T> static constexpr std::size_t maximumElements() noexcept
    {
        const auto difference = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
        return (std::min)(auditedGcc ? difference / sizeof(T) : difference,
            (std::numeric_limits<std::size_t>::max)() / sizeof(T));
    }
    template<class T>
    static ::media::Result<std::size_t> request(std::size_t count)
    {
        if (!count) return ::media::Result<std::size_t>::success(0);
        auto bytes = MediaCheckedArithmetic::multiply(count, sizeof(T), "PMR container request");
        if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Result<std::size_t>::failure(::media::ErrorInfo::allocationFailed("PMR container request overflow"));
        return AllocationCharge::allocationFootprint(static_cast<std::size_t>(bytes.value()), alignof(T));
    }

    static ::media::Result<std::size_t> total(std::size_t count, std::size_t unit,
        std::size_t proxy, std::size_t first, std::size_t second)
    {
        auto bytes = MediaCheckedArithmetic::multiply(count, unit, "PMR container blocks");
        for (auto extra : {proxy, first, second}) {
            if (!bytes) return ::media::Result<std::size_t>::failure(bytes.error());
            bytes = MediaCheckedArithmetic::add(bytes.value(), extra, "PMR container overlap");
        }
        if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Result<std::size_t>::failure(::media::ErrorInfo::allocationFailed("PMR container overlap overflow"));
        return ::media::Result<std::size_t>::success(static_cast<std::size_t>(bytes.value()));
    }
};

struct MediaStandardAllocationRequest final {
    static ::media::Result<std::size_t> allocationFootprint(std::size_t bytes, std::size_t)
    { return ::media::Result<std::size_t>::success(bytes); }
};
using MediaPmrContainerGeometry = MediaContainerGeometry<MediaPreparationMemoryResource>;
using MediaStandardContainerGeometry = MediaContainerGeometry<MediaStandardAllocationRequest>;

} // namespace media::ffmpeg::graph
