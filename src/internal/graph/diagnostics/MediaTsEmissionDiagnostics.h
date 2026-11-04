#pragma once

#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace media::ffmpeg::graph {

struct MediaTsEmissionSnapshot final {
    std::uint64_t producedDatagrams = 0;
    std::uint64_t producedTsPayloadBytes = 0;
    std::size_t pendingBytes = 0;
    std::size_t peakPendingBytes = 0;
    std::uint64_t accessUnits = 0;
    std::int64_t currentSchedulingDebtNanoseconds = 0;
    std::int64_t maximumSchedulingDebtNanoseconds = 0;
    std::int64_t maximumMuxInputAfterEmitNanoseconds = 0;
    std::int64_t maximumMuxInputAfterDispatchNanoseconds = 0;
    std::int64_t worstMuxInputReadyNanoseconds = 0;
    std::int64_t worstMuxInputEmitNanoseconds = 0;
    std::int64_t worstMuxInputDispatchNanoseconds = 0;
    std::int64_t maximumProtocolBatchProducedAfterReleaseNanoseconds = 0;
    std::int64_t worstProtocolBatchProducedNanoseconds = 0;
    std::int64_t worstProtocolBatchReleaseNanoseconds = 0;
    std::int64_t worstProtocolBatchDeadlineNanoseconds = 0;
    std::uint64_t worstProtocolBatchDatagrams = 0;
};

class MediaTsEmissionDiagnostics final {
public:
    void recordPendingBytes(std::size_t bytes) noexcept;
    void recordAccessUnitDecision(
        const MediaTsAccessUnitEmissionDecision& decision) noexcept;
    void recordAccessUnitReady(
        MediaRunningTime ready,
        MediaRunningTime emit,
        MediaRunningTime dispatch) noexcept;
    void recordProtocolBatchProduced(
        MediaRunningTime produced,
        MediaRunningTime release,
        MediaRunningTime deadline,
        std::uint64_t datagrams,
        std::uint64_t tsPayloadBytes) noexcept;
    void recordAccessUnitCompleted() noexcept;

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
