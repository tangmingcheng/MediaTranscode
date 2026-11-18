#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace media::ffmpeg::graph {

// Read back retained ordinary-allocator requests, not a future growth bound.
// The object itself is accounted by its owner. No PMR allocation header exists
// for these requests; heap implementation overhead is outside this contract.
class MediaStandardContainerStorageReadback final {
public:
    // Fixed composition preparation transfers ordinary-allocator backing by
    // move. Debug iterator-proxy transfer peaks are not covered by this model.
    static ::media::Status validatePreparationMoves()
    {
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && !_HAS_CXX23 && _ITERATOR_DEBUG_LEVEL == 0
        return ::media::Status::success();
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        return ::media::Status::success();
#else
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Fixed preparation storage requires audited release container moves"));
#endif
    }

    // Assignment into a newly default-constructed empty string.
    static ::media::Result<std::size_t> stringAssignEmpty(std::size_t count)
    {
        auto copy = stringCopy(count);
        if (!copy) return copy;
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        const std::string empty;
        if (count <= empty.capacity()) return copy;
        // _M_mutate -> _M_create(count, old_capacity): geometric growth
        // differs from fresh _M_construct(count, 0).
        return total((std::max)(count, empty.capacity() * 2) + 1, 0);
#else
        return copy;
#endif
    }

    // const string + const char*: MSVC constructs the concatenation once;
    // libstdc++ copies the left operand then appends, retaining both allocations
    // during growth. Return the construction peak, not just final capacity.
    static ::media::Result<std::size_t> stringConcatenation(std::size_t left, std::size_t right)
    {
        auto length = MediaCheckedArithmetic::add(left, right, "string concatenation length");
        if (!length) return ::media::Result<std::size_t>::failure(length.error());
        if (length.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Result<std::size_t>::failure(::media::ErrorInfo::allocationFailed("String length overflows"));
        auto result = stringCopy(static_cast<std::size_t>(length.value()));
        if (!result) return result;
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        const std::string empty;
        const auto oldCapacity = (std::max)(left, empty.capacity());
        auto initial = stringCopy(left);
        if (!initial || length.value() <= oldCapacity) return initial;
        const auto doubled = oldCapacity > empty.max_size() - oldCapacity
            ? empty.max_size() : oldCapacity * 2;
        return total((std::max)(static_cast<std::size_t>(length.value()), doubled) + 1, initial.value());
#else
        return result;
#endif
    }

    // Fresh ordinary vector followed by reserve(count), with no live elements.
    template<class T> static ::media::Result<std::size_t> vectorReserveEmpty(std::size_t count)
    {
        static_assert(!std::is_same_v<T, bool>);
        auto proxy = proxyBytes();
        if (!proxy) return proxy;
        auto bytes = MediaCheckedArithmetic::multiply(count, sizeof(T), "fresh vector reservation");
        if (!bytes) return ::media::Result<std::size_t>::failure(bytes.error());
        return total(bytes.value(), proxy.value());
    }

    static ::media::Result<std::size_t> string(const std::string& value)
    {
        auto proxy = proxyBytes();
        if (!proxy) return proxy;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
        // xstring::_Large_mode_engaged and _Allocate_for_capacity. The latter
        // stores allocated character count minus the null terminator in capacity.
        constexpr auto small = std::_String_val<std::_Simple_types<char>>::_Small_string_capacity;
        if (value.capacity() <= small) return proxy;
        auto characters = MediaCheckedArithmetic::add(value.capacity(), 1,
            "ordinary string terminator allocation");
        if (!characters) return ::media::Result<std::size_t>::failure(characters.error());
        return total(characters.value(), proxy.value());
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        // GCC basic_string::_M_is_local/capacity: the empty string exposes
        // its inline capacity without allocating. Nonlocal storage retains
        // exactly capacity + 1 chars, including the terminator.
        if (value.capacity() <= std::string{}.capacity()) return proxy;
        auto characters = MediaCheckedArithmetic::add(value.capacity(), 1,
            "ordinary string terminator allocation");
        if (!characters) return ::media::Result<std::size_t>::failure(characters.error());
        return total(characters.value(), proxy.value());
#else
        return proxy;
#endif
    }

    // Fresh string copy/range construction, not retained capacity or append.
    // xstring::_Construct -> _Calculate_growth(count, SSO, max_size).
    static ::media::Result<std::size_t> stringCopy(std::size_t count)
    {
        auto proxy = proxyBytes();
        if (!proxy) return proxy;
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L
        using Value = std::_String_val<std::_Simple_types<char>>;
        constexpr auto small = Value::_Small_string_capacity;
        constexpr auto maximum = (std::min)(static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()),
            (std::numeric_limits<std::size_t>::max)() - 1);
        if (count > maximum) return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::allocationFailed("String copy exceeds allocator max_size"));
        if (count <= small) return proxy;
        const auto masked = count | Value::_Alloc_mask;
        const auto capacity = masked > maximum ? maximum : (std::max)(masked, small + small / 2);
        return total(capacity + 1, proxy.value());
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        // GCC basic_string::_M_construct(forward_iterator_tag) calls
        // _M_create(count, 0); the fresh copy has no geometric growth.
        const std::string empty;
        if (count > empty.max_size()) return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::allocationFailed("String copy exceeds allocator max_size"));
        if (count <= empty.capacity()) return proxy;
        auto characters = MediaCheckedArithmetic::add(count, 1,
            "ordinary string copy terminator allocation");
        if (!characters) return ::media::Result<std::size_t>::failure(characters.error());
        return total(characters.value(), proxy.value());
#else
        return proxy;
#endif
    }

    template<class T>
    static ::media::Result<std::size_t> vector(const std::vector<T>& value)
    {
        static_assert(!std::is_same_v<T, bool>, "Packed bit storage requires its own allocation contract");
        auto proxy = proxyBytes();
        if (!proxy) return proxy;
        auto bytes = MediaCheckedArithmetic::multiply(value.capacity(), sizeof(T),
            "ordinary vector retained allocation");
        if (!bytes) return ::media::Result<std::size_t>::failure(bytes.error());
        return total(bytes.value(), proxy.value());
    }

private:
    static ::media::Result<std::size_t> proxyBytes()
    {
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && !_HAS_CXX23
#if _ITERATOR_DEBUG_LEVEL == 0
        return ::media::Result<std::size_t>::success(0);
#else
        return ::media::Result<std::size_t>::success(sizeof(std::_Container_proxy));
#endif
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)
        // Non-debug libstdc++ string/vector have no separately allocated
        // iterator proxy. Vector capacity is its sole retained T allocation.
        return ::media::Result<std::size_t>::success(0);
#else
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::unsupported(
            "Ordinary container readback requires an audited C++20 standard library"));
#endif
    }

    static ::media::Result<std::size_t> total(std::uint64_t backing, std::size_t proxy)
    {
        auto sum = MediaCheckedArithmetic::add(backing, proxy, "ordinary container allocation");
        if (!sum) return ::media::Result<std::size_t>::failure(sum.error());
        if (sum.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Result<std::size_t>::failure(::media::ErrorInfo::allocationFailed(
                "Ordinary container allocation exceeds addressable memory"));
        return ::media::Result<std::size_t>::success(static_cast<std::size_t>(sum.value()));
    }
};

} // namespace media::ffmpeg::graph
