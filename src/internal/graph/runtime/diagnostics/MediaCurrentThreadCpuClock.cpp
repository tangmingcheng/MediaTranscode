#include "internal/graph/runtime/diagnostics/MediaCurrentThreadCpuClock.h"

#include <limits>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <ctime>
#endif

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t>
MediaCurrentThreadCpuClock::nowNanoseconds() noexcept
{
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!::GetThreadTimes(
            ::GetCurrentThread(), &created, &exited, &kernel, &user)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::ioFailure(
                "GetThreadTimes failed",
                static_cast<int>(::GetLastError())));
    }
    ULARGE_INTEGER kernelTicks{};
    kernelTicks.LowPart = kernel.dwLowDateTime;
    kernelTicks.HighPart = kernel.dwHighDateTime;
    ULARGE_INTEGER userTicks{};
    userTicks.LowPart = user.dwLowDateTime;
    userTicks.HighPart = user.dwHighDateTime;
    if (kernelTicks.QuadPart >
        (std::numeric_limits<std::uint64_t>::max)() - userTicks.QuadPart) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::internalError(
                "thread CPU counter overflowed"));
    }
    const std::uint64_t ticks = kernelTicks.QuadPart + userTicks.QuadPart;
    constexpr std::uint64_t NanosecondsPerFileTimeTick = 100;
    if (ticks > (std::numeric_limits<std::uint64_t>::max)() /
                    NanosecondsPerFileTimeTick) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::internalError(
                "thread CPU nanoseconds overflowed"));
    }
    return ::media::Result<std::uint64_t>::success(
        ticks * NanosecondsPerFileTimeTick);
#elif defined(__linux__)
    timespec value{};
    if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::ioFailure(
                "clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed", errno));
    }
    if (value.tv_sec < 0 || value.tv_nsec < 0 ||
        value.tv_nsec >= 1'000'000'000L) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::internalError(
                "thread CPU clock returned an invalid timespec"));
    }
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000ULL;
    const auto seconds = static_cast<std::uint64_t>(value.tv_sec);
    if (seconds > (std::numeric_limits<std::uint64_t>::max)() /
                      NanosecondsPerSecond) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::internalError(
                "thread CPU nanoseconds overflowed"));
    }
    return ::media::Result<std::uint64_t>::success(
        seconds * NanosecondsPerSecond +
        static_cast<std::uint64_t>(value.tv_nsec));
#else
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "current thread CPU clock is unsupported on this platform"));
#endif
}

} // namespace media::ffmpeg::graph
