#include "internal/graph/runtime/resource/MediaThreadAllocationGeometry.h"
#include <new>
#include <thread>
#include <tuple>

namespace media::ffmpeg::graph {
namespace {
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
struct InvocationTag {
    friend constexpr std::size_t allocationBytes(InvocationTag);
    friend constexpr std::size_t allocationAlignment(InvocationTag);
};
struct StopStateTag {
    friend constexpr std::size_t allocationBytes(StopStateTag);
    friend constexpr std::size_t allocationAlignment(StopStateTag);
};
// [temp.explicit]: access checking does not apply to names used to specify
// explicit instantiations. Read the audited implementation's real types in
// this translation unit only; never reproduce their private object layouts.
template<class Tag, class T> struct AllocationReadback {
    friend constexpr std::size_t allocationBytes(Tag) { return sizeof(T); }
    friend constexpr std::size_t allocationAlignment(Tag) { return alignof(T); }
};
template struct AllocationReadback<InvocationTag,
    std::thread::_State_impl<std::thread::_Call_wrapper<MediaThreadWorkerEntry, std::stop_token>>>;
template struct AllocationReadback<StopStateTag, std::stop_token::_Stop_state_t>;
#endif
} // namespace

::media::Result<MediaThreadAllocationGeometry> MediaThreadAllocationGeometry::current()
{
    using Result = ::media::Result<MediaThreadAllocationGeometry>;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
    // Audited thread::_Start allocates this tuple; stop_source allocates _Stop_state.
    using Invocation = std::tuple<MediaThreadWorkerEntry, std::stop_token>;
    constexpr MediaThreadAllocationGeometry geometry{
        sizeof(Invocation), alignof(Invocation), sizeof(std::_Stop_state), alignof(std::_Stop_state)};
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    constexpr MediaThreadAllocationGeometry geometry{
        allocationBytes(InvocationTag{}), allocationAlignment(InvocationTag{}),
        allocationBytes(StopStateTag{}), allocationAlignment(StopStateTag{})};
#else
    return Result::failure(::media::ErrorInfo::unsupported(
        "Prepared thread allocation requires an audited standard-library implementation"));
#endif
#if (defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L) ||     (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG))
    static_assert(geometry.invocationAlignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    static_assert(geometry.stopStateAlignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    return Result::success(geometry);
#endif
}
} // namespace media::ffmpeg::graph
