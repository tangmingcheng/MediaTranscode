#pragma once

#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace media::ffmpeg::graph {

struct MediaTsEmissionSnapshot final {
    std::uint64_t datagrams = 0;
    std::uint64_t wireBytes = 0;
    std::uint64_t immediateDeadlines = 0;
    std::uint64_t deferredDeadlines = 0;
    std::int64_t plannedWaitNanoseconds = 0;
    std::uint64_t lateDatagrams = 0;
    std::int64_t maximumLatenessNanoseconds = 0;
    std::size_t pendingBytes = 0;
    std::size_t peakPendingBytes = 0;
    std::uint64_t pressureFailures = 0;
};

class MediaTsEmissionDiagnostics final {
public:
    void recordCommittedDatagram(
        std::size_t wireBytes,
        MediaRunningTime plannedWait,
        MediaRunningTime deadline,
        MediaRunningTime actualEmission) noexcept;
    void recordPendingBytes(std::size_t bytes) noexcept;
    void recordPressureFailure() noexcept;

    const MediaTsEmissionSnapshot& snapshot() const noexcept;
    void logPlan(
        const MediaTsDatagramEmissionPlan& plan,
        std::uint64_t generation) const;
    void logSnapshot(
        std::string_view stage,
        std::string_view exitReason,
        std::uint64_t generation) const;

private:
    MediaTsEmissionSnapshot m_snapshot;
};

} // namespace media::ffmpeg::graph
