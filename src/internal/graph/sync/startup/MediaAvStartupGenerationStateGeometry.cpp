#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <bit>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && _ITERATOR_DEBUG_LEVEL == 0
template<class Container>
::media::Result<MediaAvStartupHashGeometry> hashGeometry(std::size_t buckets)
{
    using R = ::media::Result<MediaAvStartupHashGeometry>;
    // xhash:268-313 owns a list node and two unchecked iterators per bucket.
    // Its initial table has eight buckets; rehash allocates the new table
    // before releasing the old one. These are layout facts, not media limits.
    using Node = std::_List_node<typename Container::value_type, void*>;
    using Cell = typename Container::_Unchecked_iterator;
    static_assert(std::is_same_v<typename std::allocator_traits<typename Container::allocator_type>::void_pointer, void*>);
    if (buckets < 8 || !std::has_single_bit(buckets))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup hash requires audited power-of-two bucket shape"));
    auto cells = MediaCheckedArithmetic::multiply(buckets, 2, "Startup hash allocator cells");
    if (!cells) return R::failure(cells.error());
    if (cells.value() > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / sizeof(Cell))
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash bucket request exceeds allocator maximum"));
    const auto finalBytes = static_cast<std::size_t>(cells.value()) * sizeof(Cell);
    return R::success({{sizeof(Node), alignof(Node)}, {16 * sizeof(Cell), alignof(Cell)},
        {finalBytes, alignof(Cell)}, 8, buckets, 1.0f});
}
#endif
}
::media::Result<std::array<MediaAvStartupHashGeometry, 2>> MediaAvStartupGenerationState::allocationGeometry(
    std::size_t buckets)
{
    using R = ::media::Result<std::array<MediaAvStartupHashGeometry, 2>>;
#if !defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION != 145 || _MSVC_STL_UPDATE != 202604L || _ITERATOR_DEBUG_LEVEL != 0
    return R::failure(::media::ErrorInfo::unsupported("Prepared startup hash geometry requires audited MSVC STL 145/202604 without iterator debugging"));
#else
    auto payloads = hashGeometry<Payloads>(buckets);
    if (!payloads) return R::failure(payloads.error());
    auto seen = hashGeometry<Seen>(buckets);
    if (!seen) return R::failure(seen.error());
    return R::success({payloads.value(), seen.value()});
#endif
}
::media::Result<MediaSharedAllocationGeometry> MediaAvStartupGenerationState::sharedAllocationGeometry()
{
    return probeMediaSharedAllocationGeometry<MediaAvStartupGenerationState>(
        std::type_identity<AllocationAllocator>{});
}
} // namespace media::ffmpeg::graph
