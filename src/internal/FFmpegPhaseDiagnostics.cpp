#include "internal/FFmpegPhaseDiagnostics.h"

#include "spdlog/spdlog.h"

#include <ctime>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace media::ffmpeg {
namespace {

#if defined(_WIN32)
    int64_t fileTimeToMs(const FILETIME& ft)
    {
        ULARGE_INTEGER value;
        value.LowPart = ft.dwLowDateTime;
        value.HighPart = ft.dwHighDateTime;
        return static_cast<int64_t>(value.QuadPart / 10000ULL);
    }
#endif

    int64_t diffOrUnavailable(int64_t before, int64_t after)
    {
        if (before < 0 || after < 0) {
            return -1;
        }
        return after - before;
    }

    std::string cpuField(const char* name, int64_t value)
    {
        std::ostringstream oss;
        oss << name << "=";
        if (value < 0) {
            oss << "unavailable";
        }
        else {
            oss << value << "ms";
        }
        return oss.str();
    }

    std::string counterDeltaField(const FFmpegPhaseDiagnostics::Counters& before,
                                  const FFmpegPhaseDiagnostics::Counters& after)
    {
        std::ostringstream oss;
        oss << "video_packets_delta=" << (after.videoPackets - before.videoPackets)
            << ", audio_packets_delta=" << (after.audioPackets - before.audioPackets)
            << ", progress_callbacks_delta=" << (after.progressCallbacks - before.progressCallbacks)
            << ", video_packets_total=" << after.videoPackets
            << ", audio_packets_total=" << after.audioPackets
            << ", progress_callbacks_total=" << after.progressCallbacks;
        return oss.str();
    }

    std::string appendDetails(const std::string& base, const std::string& details)
    {
        if (details.empty()) {
            return base;
        }
        return base + ", " + details;
    }

} // namespace

    FFmpegPhaseDiagnostics::Session::Session(const char* name)
        : m_name(name ? name : "unknown")
        , m_started(FFmpegPhaseDiagnostics::snapshot())
    {
        spdlog::info(
            "[PHASE][{}] begin process_cpu={}, thread_cpu={}",
            m_name,
            m_started.processCpuMs < 0 ? "unavailable" : std::to_string(m_started.processCpuMs) + "ms",
            m_started.threadCpuMs < 0 ? "unavailable" : std::to_string(m_started.threadCpuMs) + "ms"
        );
    }

    FFmpegPhaseDiagnostics::Snapshot FFmpegPhaseDiagnostics::Session::mark() const
    {
        return FFmpegPhaseDiagnostics::snapshot();
    }

    void FFmpegPhaseDiagnostics::Session::logStep(const char* stepName,
                                                  const Snapshot& before,
                                                  const Counters& countersBefore,
                                                  const Counters& countersAfter,
                                                  const std::string& details) const
    {
        const Snapshot after = FFmpegPhaseDiagnostics::snapshot();
        const std::string counters = counterDeltaField(countersBefore, countersAfter);
        spdlog::info(
            "[PHASE][{}] {} wall={}ms, {}, {}, {}",
            m_name,
            stepName ? stepName : "step",
            FFmpegPhaseDiagnostics::wallElapsedMs(before, after),
            cpuField("process_cpu", FFmpegPhaseDiagnostics::processCpuElapsedMs(before, after)),
            cpuField("thread_cpu", FFmpegPhaseDiagnostics::threadCpuElapsedMs(before, after)),
            appendDetails(counters, details)
        );
    }

    void FFmpegPhaseDiagnostics::Session::logFailure(const char* stepName,
                                                     const Snapshot& before,
                                                     const Counters& countersBefore,
                                                     const Counters& countersAfter,
                                                     const std::string& details) const
    {
        const Snapshot after = FFmpegPhaseDiagnostics::snapshot();
        const std::string counters = counterDeltaField(countersBefore, countersAfter);
        spdlog::warn(
            "[PHASE][{}] {} failed wall={}ms, {}, {}, {}",
            m_name,
            stepName ? stepName : "step",
            FFmpegPhaseDiagnostics::wallElapsedMs(before, after),
            cpuField("process_cpu", FFmpegPhaseDiagnostics::processCpuElapsedMs(before, after)),
            cpuField("thread_cpu", FFmpegPhaseDiagnostics::threadCpuElapsedMs(before, after)),
            appendDetails(counters, details)
        );
    }

    void FFmpegPhaseDiagnostics::Session::finish(bool success,
                                                 const Counters& countersBefore,
                                                 const Counters& countersAfter,
                                                 const std::string& details) const
    {
        const Snapshot after = FFmpegPhaseDiagnostics::snapshot();
        const std::string counters = counterDeltaField(countersBefore, countersAfter);
        spdlog::info(
            "[PHASE][{}] end status={}, wall={}ms, {}, {}, {}",
            m_name,
            success ? "ok" : "failed",
            FFmpegPhaseDiagnostics::wallElapsedMs(m_started, after),
            cpuField("process_cpu", FFmpegPhaseDiagnostics::processCpuElapsedMs(m_started, after)),
            cpuField("thread_cpu", FFmpegPhaseDiagnostics::threadCpuElapsedMs(m_started, after)),
            appendDetails(counters, details)
        );
    }

    FFmpegPhaseDiagnostics::Snapshot FFmpegPhaseDiagnostics::snapshot()
    {
        Snapshot snapshot;
        snapshot.wallTime = Clock::now();
        snapshot.processCpuMs = currentProcessCpuMs();
        snapshot.threadCpuMs = currentThreadCpuMs();
        return snapshot;
    }

    int64_t FFmpegPhaseDiagnostics::wallElapsedMs(const Snapshot& before, const Snapshot& after)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            after.wallTime - before.wallTime
        ).count();
    }

    int64_t FFmpegPhaseDiagnostics::processCpuElapsedMs(const Snapshot& before, const Snapshot& after)
    {
        return diffOrUnavailable(before.processCpuMs, after.processCpuMs);
    }

    int64_t FFmpegPhaseDiagnostics::threadCpuElapsedMs(const Snapshot& before, const Snapshot& after)
    {
        return diffOrUnavailable(before.threadCpuMs, after.threadCpuMs);
    }

    int64_t FFmpegPhaseDiagnostics::currentProcessCpuMs()
    {
#if defined(_WIN32)
        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
            return -1;
        }
        return fileTimeToMs(kernel) + fileTimeToMs(user);
#elif defined(CLOCK_PROCESS_CPUTIME_ID)
        timespec ts{};
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
            return -1;
        }
        return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#else
        const std::clock_t value = std::clock();
        if (value == static_cast<std::clock_t>(-1)) {
            return -1;
        }
        return static_cast<int64_t>(value) * 1000 / CLOCKS_PER_SEC;
#endif
    }

    int64_t FFmpegPhaseDiagnostics::currentThreadCpuMs()
    {
#if defined(_WIN32)
        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user)) {
            return -1;
        }
        return fileTimeToMs(kernel) + fileTimeToMs(user);
#elif defined(CLOCK_THREAD_CPUTIME_ID)
        timespec ts{};
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
            return -1;
        }
        return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#else
        return -1;
#endif
    }

} // namespace media::ffmpeg
