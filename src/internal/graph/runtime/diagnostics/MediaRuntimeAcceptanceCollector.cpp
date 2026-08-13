#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"

#if defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#elif defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unistd.h>
#endif

namespace media::ffmpeg::graph {
namespace {

double onlineProcessorCount() noexcept
{
#if defined(_WIN32)
    const DWORD count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return count > 0 ? static_cast<double>(count) : 1.0;
#elif defined(__linux__)
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? static_cast<double>(count) : 1.0;
#else
    return 1.0;
#endif
}

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
                result.processCpuPercent = 100.0 * onlineProcessorCount() *
                    static_cast<double>(processTotal - m_processTotal) / elapsed;
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
#elif defined(__linux__)
struct LinuxCpuCounters final {
    std::uint64_t total = 0;
    std::uint64_t idle = 0;
    std::uint64_t process = 0;
};

::media::Result<LinuxCpuCounters> readLinuxCpuCounters() noexcept
{
    FILE* systemFile = std::fopen("/proc/stat", "r");
    if (!systemFile) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::ioFailure("open /proc/stat failed", errno));
    }
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long ioWait = 0;
    unsigned long long irq = 0;
    unsigned long long softIrq = 0;
    unsigned long long steal = 0;
    const int systemFields = std::fscanf(
        systemFile, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user, &nice, &system, &idle, &ioWait, &irq, &softIrq, &steal);
    const int systemCloseError = std::fclose(systemFile);
    if (systemFields != 8 || systemCloseError != 0) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::ioFailure("read /proc/stat failed", errno));
    }

    FILE* processFile = std::fopen("/proc/self/stat", "r");
    if (!processFile) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::ioFailure("open /proc/self/stat failed", errno));
    }
    char processText[4096]{};
    const char* readResult = std::fgets(
        processText, static_cast<int>(sizeof(processText)), processFile);
    const int processCloseError = std::fclose(processFile);
    if (!readResult || processCloseError != 0) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::ioFailure("read /proc/self/stat failed", errno));
    }
    char* fields = std::strrchr(processText, ')');
    if (!fields || fields[1] != ' ') {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::internalError("invalid /proc/self/stat process name field"));
    }
    fields += 2;
    unsigned long long processUser = 0;
    unsigned long long processSystem = 0;
    char* context = nullptr;
    char* field = strtok_r(fields, " ", &context);
    std::size_t fieldNumber = 3;
    while (field) {
        if (fieldNumber == 14 || fieldNumber == 15) {
            char* end = nullptr;
            errno = 0;
            const unsigned long long value = std::strtoull(field, &end, 10);
            if (errno != 0 || end == field || *end != '\0') {
                return ::media::Result<LinuxCpuCounters>::failure(
                    ::media::ErrorInfo::internalError(
                        "invalid /proc/self/stat CPU counter"));
            }
            if (fieldNumber == 14) processUser = value;
            else processSystem = value;
        }
        if (fieldNumber >= 15) break;
        field = strtok_r(nullptr, " ", &context);
        ++fieldNumber;
    }
    if (fieldNumber < 15 ||
        processUser > (std::numeric_limits<std::uint64_t>::max)() - processSystem) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::internalError("incomplete /proc/self/stat CPU counters"));
    }
    const std::uint64_t total = user + nice + system + idle + ioWait + irq + softIrq + steal;
    if (total < user || idle > (std::numeric_limits<std::uint64_t>::max)() - ioWait) {
        return ::media::Result<LinuxCpuCounters>::failure(
            ::media::ErrorInfo::internalError("overflowed /proc/stat CPU counters"));
    }
    return ::media::Result<LinuxCpuCounters>::success(
        LinuxCpuCounters{total, idle + ioWait, processUser + processSystem});
}

::media::Result<MediaRuntimePlatformSample> readLinuxProcessStatus() noexcept
{
    FILE* statusFile = std::fopen("/proc/self/status", "r");
    if (!statusFile) {
        return ::media::Result<MediaRuntimePlatformSample>::failure(
            ::media::ErrorInfo::ioFailure("open /proc/self/status failed", errno));
    }
    MediaRuntimePlatformSample result;
    bool foundMemory = false;
    bool foundThreads = false;
    char line[512]{};
    while (std::fgets(line, static_cast<int>(sizeof(line)), statusFile)) {
        unsigned long long residentKilobytes = 0;
        unsigned long long threadCount = 0;
        if (std::sscanf(line, "VmRSS: %llu kB", &residentKilobytes) == 1) {
            if (residentKilobytes >
                (std::numeric_limits<std::uint64_t>::max)() / 1024U) {
                std::fclose(statusFile);
                return ::media::Result<MediaRuntimePlatformSample>::failure(
                    ::media::ErrorInfo::internalError("VmRSS exceeds byte range"));
            }
            result.workingSetBytes = residentKilobytes * 1024U;
            foundMemory = true;
        } else if (std::sscanf(line, "Threads: %llu", &threadCount) == 1) {
            if (threadCount > (std::numeric_limits<std::size_t>::max)()) {
                std::fclose(statusFile);
                return ::media::Result<MediaRuntimePlatformSample>::failure(
                    ::media::ErrorInfo::internalError("thread count exceeds size range"));
            }
            result.threadCount = static_cast<std::size_t>(threadCount);
            foundThreads = true;
        }
    }
    const int closeError = std::fclose(statusFile);
    if (closeError != 0 || !foundMemory || !foundThreads) {
        return ::media::Result<MediaRuntimePlatformSample>::failure(
            ::media::ErrorInfo::ioFailure("read /proc/self/status failed", errno));
    }
    return ::media::Result<MediaRuntimePlatformSample>::success(result);
}

class LinuxRuntimePlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        auto counters = readLinuxCpuCounters();
        if (!counters) {
            return ::media::Result<MediaRuntimePlatformSample>::failure(counters.error());
        }
        auto status = readLinuxProcessStatus();
        if (!status) return status;
        MediaRuntimePlatformSample result = status.value();
        if (m_hasPrevious) {
            if (counters.value().total < m_previous.total ||
                counters.value().idle < m_previous.idle ||
                counters.value().process < m_previous.process) {
                return ::media::Result<MediaRuntimePlatformSample>::failure(
                    ::media::ErrorInfo::internalError(
                        "Linux CPU counters did not advance monotonically"));
            }
            const std::uint64_t totalDelta = counters.value().total - m_previous.total;
            if (totalDelta > 0) {
                const std::uint64_t idleDelta = counters.value().idle - m_previous.idle;
                result.systemCpuPercent = 100.0 *
                    static_cast<double>(totalDelta - idleDelta) /
                    static_cast<double>(totalDelta);
                result.processCpuPercent = 100.0 * onlineProcessorCount() *
                    static_cast<double>(counters.value().process - m_previous.process) /
                    static_cast<double>(totalDelta);
                result.cpuValid = true;
            }
        }
        m_previous = counters.value();
        m_hasPrevious = true;
        return ::media::Result<MediaRuntimePlatformSample>::success(result);
    }

private:
    LinuxCpuCounters m_previous;
    bool m_hasPrevious = false;
};
#else
class UnsupportedRuntimePlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        return ::media::Result<MediaRuntimePlatformSample>::failure(
            ::media::ErrorInfo::unsupported(
                "runtime platform sampling is unsupported on this platform"));
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
#if defined(_WIN32)
    return std::make_unique<WindowsRuntimePlatformSampler>();
#elif defined(__linux__)
    return std::make_unique<LinuxRuntimePlatformSampler>();
#else
    return std::make_unique<UnsupportedRuntimePlatformSampler>();
#endif
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
