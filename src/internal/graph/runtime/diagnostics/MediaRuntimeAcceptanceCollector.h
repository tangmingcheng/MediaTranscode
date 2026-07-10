#pragma once

#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include "media_transcode/Result.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaRuntimeAcceptanceClock {
public:
    using time_point = std::chrono::steady_clock::time_point;
    virtual ~MediaRuntimeAcceptanceClock() = default;
    virtual time_point now() const noexcept = 0;
};

struct MediaRuntimePlatformSample {
    double systemCpuPercent = 0.0;
    double processCpuPercent = 0.0;
    std::size_t threadCount = 0;
    std::uint64_t workingSetBytes = 0;
    bool cpuValid = false;
};

class MediaRuntimePlatformSampler {
public:
    virtual ~MediaRuntimePlatformSampler() = default;
    virtual ::media::Result<MediaRuntimePlatformSample> sample() noexcept = 0;
};

std::unique_ptr<MediaRuntimeAcceptanceClock> createSteadyAcceptanceClock();
std::unique_ptr<MediaRuntimePlatformSampler> createPlatformRuntimeSampler();

class MediaRuntimeAcceptanceCollector final {
public:
    explicit MediaRuntimeAcceptanceCollector(
        std::unique_ptr<MediaRuntimeAcceptanceClock> clock = createSteadyAcceptanceClock(),
        std::unique_ptr<MediaRuntimePlatformSampler> platform = createPlatformRuntimeSampler(),
        std::chrono::milliseconds stallThreshold = std::chrono::seconds(5));

    ::media::Status sample(std::uint64_t progress) noexcept;
    void recordError() noexcept;
    MediaGraphRuntimeMetrics snapshot() const noexcept;
    void reset() noexcept;

private:
    std::unique_ptr<MediaRuntimeAcceptanceClock> m_clock;
    std::unique_ptr<MediaRuntimePlatformSampler> m_platform;
    std::chrono::milliseconds m_stallThreshold;
    mutable std::mutex m_mutex;
    MediaGraphRuntimeMetrics m_metrics;
    MediaRuntimeAcceptanceClock::time_point m_lastProgressAt{};
    MediaRuntimeAcceptanceClock::time_point m_lastStallAt{};
    std::uint64_t m_lastProgress = 0;
    bool m_hasProgress = false;
};

} // namespace media::ffmpeg::graph
