#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && _ITERATOR_DEBUG_LEVEL == 0
template<class Container>
::media::Result<MediaAvStartupHashGeometry> hashGeometry(std::size_t maximum)
{
    using R = ::media::Result<MediaAvStartupHashGeometry>;
    // xhash owns one list head and two unchecked iterators per bucket.
    using Node = std::_List_node<typename Container::value_type, void*>;
    using Cell = typename Container::_Unchecked_iterator;
    static_assert(std::is_same_v<typename std::allocator_traits<typename Container::allocator_type>::void_pointer, void*>);
    const auto required = (std::max)(std::size_t{8}, maximum);
    constexpr auto largestPower = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    if (required > largestPower)
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash power-of-two capacity overflows"));
    const auto buckets = std::bit_ceil(required);
    auto cells = MediaCheckedArithmetic::multiply(buckets, 2, "Startup hash allocator cells");
    if (!cells) return R::failure(cells.error());
    if (cells.value() > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / sizeof(Cell))
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash bucket request exceeds allocator maximum"));
    const auto finalBytes = static_cast<std::size_t>(cells.value()) * sizeof(Cell);
    return R::success({{sizeof(Node), alignof(Node)},
        MediaAvStartupIndexGeometry{16 * sizeof(Cell), alignof(Cell)},
        {finalBytes, alignof(Cell)}, 1, 8, buckets, 1.0f});
}
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
template<class Container>
::media::Result<MediaAvStartupHashGeometry> hashGeometry(std::size_t maximum)
{
    using R = ::media::Result<MediaAvStartupHashGeometry>;
    using Node = std::__detail::_Hash_node<typename Container::value_type,
        std::__cache_default<typename Container::key_type, typename Container::hasher>::value>;
    using Cell = std::__detail::_Hash_node_base*;
    const auto allocatorMaximum = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / sizeof(Cell);
    if (maximum > allocatorMaximum)
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash bucket request exceeds allocator maximum"));
    std::__detail::_Prime_rehash_policy policy;
    // Mirror empty _Hashtable::rehash: max(request, buckets_for(size + 1)),
    // followed by the linked library's prime policy. No container is allocated.
    const auto required = (std::max)(maximum, policy._M_bkt_for_elements(1));
    const auto buckets = policy._M_next_bkt(required);
    if (buckets < required || buckets > allocatorMaximum)
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash prime capacity exceeds allocator maximum"));
    // _M_before_begin and _M_single_bucket are embedded in the empty table.
    return R::success({{sizeof(Node), alignof(Node)}, std::nullopt,
        {buckets * sizeof(Cell), alignof(Cell)}, 0, 1, buckets, policy.max_load_factor()});
}
#endif
}
::media::Result<std::array<MediaAvStartupHashGeometry, 2>> MediaAvStartupGenerationState::allocationGeometry(
    std::size_t maximumElements)
{
    using R = ::media::Result<std::array<MediaAvStartupHashGeometry, 2>>;
    if (!maximumElements)
        return R::failure(::media::ErrorInfo::invalidArgument("Startup hash requires a positive element bound"));
#if (defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && _ITERATOR_DEBUG_LEVEL == 0) || \
    (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG))
    auto payloads = hashGeometry<Payloads>(maximumElements);
    if (!payloads) return R::failure(payloads.error());
    auto seen = hashGeometry<Seen>(maximumElements);
    if (!seen) return R::failure(seen.error());
    return R::success({payloads.value(), seen.value()});
#else
    return R::failure(::media::ErrorInfo::unsupported(
        "Prepared startup hash geometry requires audited non-debug MSVC or GCC STL"));
#endif
}
::media::Result<MediaSharedAllocationGeometry> MediaAvStartupGenerationState::sharedAllocationGeometry()
{
    return probeMediaSharedAllocationGeometry<MediaAvStartupGenerationState>(
        std::type_identity<AllocationAllocator>{});
}
} // namespace media::ffmpeg::graph
