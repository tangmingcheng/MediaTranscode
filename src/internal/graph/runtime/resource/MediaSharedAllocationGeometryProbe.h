#pragma once
#include "internal/graph/runtime/resource/MediaSharedAllocationAllocator.h"
#include <memory>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace shared_allocation_detail {
struct ConstructionReached final {};
template<class Argument>
struct UnconstructedArgument final {
    operator Argument() const { throw ConstructionReached{}; }
};
}

// Exact allocator type matters: the shared control stores its rebound allocator.
// This overload describes the audited ordinary non-array allocate_shared path;
// unlike the reservation allocator probe below, it performs no allocation.
template<class Object, class ActualAllocator>
::media::Result<MediaSharedAllocationGeometry> probeMediaSharedAllocationGeometry(
    std::type_identity<ActualAllocator>)
{
    static_assert(std::is_object_v<Object> && !std::is_array_v<Object>);
    using Result = ::media::Result<MediaSharedAllocationGeometry>;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
    // memory allocate_shared: _Ref_count_obj_alloc3, rebind, allocate(1).
    using Control = std::_Ref_count_obj_alloc3<std::remove_cv_t<Object>, ActualAllocator>;
    return Result::success({1, sizeof(Control), alignof(Control)});
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    // GCC 12.4 shared_ptr_base.h: __shared_count rebinds the actual allocator
    // to this exact control type and __allocate_guarded allocates one object.
    using Control = std::_Sp_counted_ptr_inplace<Object, ActualAllocator, __gnu_cxx::__default_lock_policy>;
    return Result::success({1, sizeof(Control), alignof(Control)});
#else
    return Result::failure(::media::ErrorInfo::unsupported(
        "Actual allocator shared geometry requires an audited standard-library capability"));
#endif
}

// make_shared uses a different control type from allocate_shared on MSVC.
template<class Object>
::media::Result<MediaSharedAllocationGeometry> probeMediaMakeSharedAllocationGeometry()
{
    static_assert(std::is_object_v<Object> && !std::is_array_v<Object>);
    using Result = ::media::Result<MediaSharedAllocationGeometry>;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
    using Control = std::_Ref_count_obj2<Object>;
    return Result::success({1, sizeof(Control), alignof(Control)});
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    // GCC shared_ptr.h make_shared passes allocator<void> to the same inplace path.
    return probeMediaSharedAllocationGeometry<Object>(std::type_identity<std::allocator<void>>{});
#else
    return Result::failure(::media::ErrorInfo::unsupported("make_shared geometry requires audited STL"));
#endif
}

// Capability of the audited non-array allocate_shared implementation.
// No real Reservation is created or consumed during discovery.
template<class Object, class... Arguments>
::media::Result<MediaSharedAllocationGeometry> probeMediaSharedAllocationGeometry()
{
    static_assert(!std::is_array_v<Object>);
    using Result = ::media::Result<MediaSharedAllocationGeometry>;
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    // Use the very allocator instantiated by the production factories. Its
    // rebound state is part of the control block, even when no probe runs.
    return probeMediaSharedAllocationGeometry<Object>(
        std::type_identity<MediaSharedAllocationAllocator<Object>>{});
#elif !defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION != 145 || _MSVC_STL_UPDATE != 202604L
    return Result::failure(::media::ErrorInfo::unsupported(
        "Shared allocation geometry requires an audited standard-library capability"));
#else
    auto state = MediaSharedAllocationState::create();
    if (!state) return Result::failure(state.error());
    try {
        auto unexpected = std::allocate_shared<Object>(
            MediaSharedAllocationAllocator<Object>{state.value()},
            shared_allocation_detail::UnconstructedArgument<Arguments>{}...);
        return Result::failure(::media::ErrorInfo::unsupported(
            "Shared allocation probe unexpectedly constructed an object"));
    } catch (const MediaSharedAllocationProbeComplete& completed) {
        if (completed.identity() != state.value().get() || !state.value()->observed())
            return Result::failure(::media::ErrorInfo::unsupported(
                "Shared allocation probe did not capture this allocation"));
        return Result::success(*state.value()->observed());
    } catch (const shared_allocation_detail::ConstructionReached&) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Standard library constructs before the probed allocation"));
    } catch (const MediaSharedAllocationMismatch&) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Shared allocation probe request is invalid or repeated"));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Shared allocation capability probe"));
    }
#endif
}
} // namespace media::ffmpeg::graph
