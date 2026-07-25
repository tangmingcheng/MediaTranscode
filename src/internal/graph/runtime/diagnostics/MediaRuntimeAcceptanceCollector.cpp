#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"

#if defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#endif

namespace media::ffmpeg::graph {
namespace {

class SteadyAcceptanceClock final : public MediaRuntimeAcceptanceClock {
public:
    time_point now() const noexcept override { return std::chrono::steady_clock::now(); }
};

#if defined(_WIN32)
std::uint64_t fileTimeValue(const FILETIME& value) noexcept
{
    ULARGE_INTEGER result{};
    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

class WindowsRuntimePlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        FILETIME idle{}, kernel{}, user{}, created{}, exited{}, processKernel{}, processUser{};
        MediaRuntimePlatformSample result;
        if (!::GetSystemTimes(&idle, &kernel, &user)) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("GetSystemTimes failed", static_cast<int>(::GetLastError())));
        }
        if (!::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &processKernel, &processUser)) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("GetProcessTimes failed", static_cast<int>(::GetLastError())));
        }
        const std::uint64_t systemTotal = fileTimeValue(kernel) + fileTimeValue(user);
        const std::uint64_t systemIdle = fileTimeValue(idle);
        const std::uint64_t processTotal = fileTimeValue(processKernel) + fileTimeValue(processUser);
        if (m_hasPrevious) {
            if (systemTotal < m_systemTotal || systemIdle < m_systemIdle || processTotal < m_processTotal) {
                return ::media::Result<MediaRuntimePlatformSample>::failure(
                    ::media::ErrorInfo::internalError("Windows CPU counters did not advance monotonically"));
            }
            if (systemTotal > m_systemTotal) {
                const double elapsed = static_cast<double>(systemTotal - m_systemTotal);
                result.systemCpuPercent = 100.0 * (elapsed - static_cast<double>(systemIdle - m_systemIdle)) / elapsed;
                result.processCpuPercent = 100.0 * static_cast<double>(processTotal - m_processTotal) / elapsed;
                result.cpuValid = true;
            }
        }
        m_systemTotal = systemTotal;
        m_systemIdle = systemIdle;
        m_processTotal = processTotal;
        m_hasPrevious = true;
        PROCESS_MEMORY_COUNTERS memory{};
        if (!::GetProcessMemoryInfo(::GetCurrentProcess(), &memory, sizeof(memory))) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("GetProcessMemoryInfo failed", static_cast<int>(::GetLastError())));
        }
        result.workingSetBytes = memory.WorkingSetSize;
        const DWORD processId = ::GetCurrentProcessId();
        HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("CreateToolhelp32Snapshot failed", static_cast<int>(::GetLastError())));
        }
        THREADENTRY32 entry{ sizeof(entry) };
        if (!::Thread32First(snapshot, &entry)) {
            const DWORD error = ::GetLastError();
            ::CloseHandle(snapshot);
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("Thread32First failed", static_cast<int>(error)));
        }
        do {
            if (entry.th32OwnerProcessID == processId) ++result.threadCount;
        } while (::Thread32Next(snapshot, &entry));
        const DWORD iterationError = ::GetLastError();
        ::CloseHandle(snapshot);
        if (iterationError != ERROR_NO_MORE_FILES) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(
                ::media::ErrorInfo::ioFailure("Thread32Next failed", static_cast<int>(iterationError)));
        }
        return ::media::Result<MediaRuntimePlatformSample>::success(result);
    }
private:
    std::uint64_t m_systemTotal = 0;
    std::uint64_t m_systemIdle = 0;
    std::uint64_t m_processTotal = 0;
    bool m_hasPrevious = false;
};
#else
class WindowsRuntimePlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        return ::media::Result<MediaRuntimePlatformSample>::failure(
            ::media::ErrorInfo::unsupported("runtime platform sampling is unsupported on this platform"));
    }
};
#endif

} // namespace

std::unique_ptr<MediaRuntimeAcceptanceClock> createSteadyAcceptanceClock()
{
    return std::make_unique<SteadyAcceptanceClock>();
}

std::unique_ptr<MediaRuntimePlatformSampler> createPlatformRuntimeSampler()
{
    return std::make_unique<WindowsRuntimePlatformSampler>();
}

MediaRuntimeAcceptanceCollector::MediaRuntimeAcceptanceCollector(
    std::unique_ptr<MediaRuntimeAcceptanceClock> clock,
    std::unique_ptr<MediaRuntimePlatformSampler> platform,
    std::chrono::milliseconds stallThreshold)
    : m_clock(std::move(clock)), m_platform(std::move(platform)), m_stallThreshold(stallThreshold)
{
}

::media::Status MediaRuntimeAcceptanceCollector::sample(std::uint64_t progress) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = m_clock->now();
    auto platformResult = m_platform->sample();
    if (!platformResult) {
        ++m_metrics.errorCount;
        return ::media::Status::failure(platformResult.error());
    }
    const MediaRuntimePlatformSample& platform = platformResult.value();
    if (platform.cpuValid) {
        const double count = static_cast<double>(m_metrics.cpuSampleCount);
        ++m_metrics.cpuSampleCount;
        m_metrics.averageCpuPercent = (m_metrics.averageCpuPercent * count + platform.systemCpuPercent) / (count + 1.0);
        m_metrics.averageProcessCpuPercent = (m_metrics.averageProcessCpuPercent * count + platform.processCpuPercent) / (count + 1.0);
    }
    m_metrics.processThreadCount = platform.threadCount;
    m_metrics.workingSetBytes = platform.workingSetBytes;
    if (!m_hasProgress || progress != m_lastProgress) {
        m_lastProgress = progress;
        m_lastProgressAt = now;
        m_lastStallAt = now;
        m_hasProgress = true;
    } else if (now - m_lastProgressAt > m_stallThreshold && now - m_lastStallAt >= m_stallThreshold) {
        ++m_metrics.stalledIntervals;
        m_lastStallAt = now;
    }
    return ::media::Status::success();
}

void MediaRuntimeAcceptanceCollector::recordError() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_metrics.errorCount;
}

MediaGraphRuntimeMetrics MediaRuntimeAcceptanceCollector::snapshot() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

void MediaRuntimeAcceptanceCollector::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics = {};
    m_hasProgress = false;
    m_lastProgress = 0;
}

} // namespace media::ffmpeg::graph
