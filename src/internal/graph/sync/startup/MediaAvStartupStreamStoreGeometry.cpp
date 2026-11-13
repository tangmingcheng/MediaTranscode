#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include <type_traits>

namespace media::ffmpeg::graph {
::media::Result<MediaAvStartupIndexGeometries> MediaAvStartupStreamStore::allocationGeometry()
{
    using R = ::media::Result<MediaAvStartupIndexGeometries>;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && _ITERATOR_DEBUG_LEVEL == 0
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
    constexpr std::size_t emptyHeadRequests = 1;
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    // GCC 12.4 list/tree headers are embedded; only entries allocate nodes.
    using ArrivalNode = std::_List_node<Arrival::value_type>;
    using PresentationNode = std::_Rb_tree_node<Presentation::value_type>;
    using CoverageNode = std::_Rb_tree_node<Coverage::value_type>;
    using PendingNode = std::_Rb_tree_node<Pending::value_type>;
    constexpr std::size_t emptyHeadRequests = 0;
#else
    return R::failure(::media::ErrorInfo::unsupported(
        "Prepared startup index geometry requires audited non-debug MSVC or GCC STL"));
#endif
#if (defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && _ITERATOR_DEBUG_LEVEL == 0) || \
    (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG))
    return R::success({{{{sizeof(ArrivalNode), alignof(ArrivalNode)}, emptyHeadRequests},
        {{sizeof(PresentationNode), alignof(PresentationNode)}, emptyHeadRequests},
        {{sizeof(CoverageNode), alignof(CoverageNode)}, emptyHeadRequests},
        {{sizeof(PendingNode), alignof(PendingNode)}, emptyHeadRequests}}});
#endif
}
} // namespace media::ffmpeg::graph
