#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include <type_traits>

namespace media::ffmpeg::graph {
::media::Result<MediaAvStartupIndexGeometries> MediaAvStartupStreamStore::allocationGeometry()
{
    using R = ::media::Result<MediaAvStartupIndexGeometries>;
#if !defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION != 145 || _MSVC_STL_UPDATE != 202604L || _ITERATOR_DEBUG_LEVEL != 0
    return R::failure(::media::ErrorInfo::unsupported(
        "Prepared startup index geometry requires audited MSVC STL 145/202604 without iterator debugging"));
#else
    // list:778 and xtree:872 use these exact rebound node types. Empty heads
    // and inserted nodes each allocate(1); balancing allocates nothing. Query
    // the actual types without constructing even a candidate container.
    using ArrivalNode = std::_List_node<Arrival::value_type, void*>;
    using PresentationNode = std::_Tree_node<Presentation::value_type, void*>;
    using CoverageNode = std::_Tree_node<Coverage::value_type, void*>;
    using PendingNode = std::_Tree_node<Pending::value_type, void*>;
    static_assert(std::is_same_v<std::allocator_traits<Arrival::allocator_type>::void_pointer, void*>);
    static_assert(std::is_same_v<std::allocator_traits<Presentation::allocator_type>::void_pointer, void*>);
    static_assert(std::is_same_v<std::allocator_traits<Coverage::allocator_type>::void_pointer, void*>);
    static_assert(std::is_same_v<std::allocator_traits<Pending::allocator_type>::void_pointer, void*>);
    return R::success({{{sizeof(ArrivalNode), alignof(ArrivalNode)},
        {sizeof(PresentationNode), alignof(PresentationNode)},
        {sizeof(CoverageNode), alignof(CoverageNode)},
        {sizeof(PendingNode), alignof(PendingNode)}}});
#endif
}
} // namespace media::ffmpeg::graph
