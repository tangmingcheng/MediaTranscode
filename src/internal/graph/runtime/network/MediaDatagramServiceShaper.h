#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDatagramServiceShaperTelemetry final {
    std::uint64_t admittedBatches = 0;
    std::uint64_t admittedDatagrams = 0;
    std::uint64_t admittedPayloadBytes = 0;
    std::uint64_t admittedWireBytes = 0;
    std::int64_t maximumDebtDelayNanoseconds = 0;
    std::int64_t maximumArrivalAfterReleaseNanoseconds = 0;
    std::uint64_t worstArrivalGlobalSequence = 0;
    std::int64_t worstArrivalReleaseNanoseconds = 0;
    std::int64_t worstArrivalDeadlineNanoseconds = 0;
    std::int64_t worstArrivalNowNanoseconds = 0;
    std::uint64_t lastAdmittedBatchFirstSequence = 0;
    std::uint64_t lastAdmittedBatchLastSequence = 0;
    std::int64_t lastAdmittedBatchFirstReleaseNanoseconds = 0;
    std::int64_t lastAdmittedBatchLastReleaseNanoseconds = 0;
    std::int64_t lastAdmittedBatchFirstDeadlineNanoseconds = 0;
    std::int64_t lastAdmittedBatchLastDeadlineNanoseconds = 0;
    std::int64_t lastAdmittedBatchArrivalNanoseconds = 0;
    std::uint64_t targetResidenceMissedBatches = 0;
    std::uint64_t serviceCurveViolations = 0;
    std::uint64_t deadlineMisses = 0;
    std::uint64_t pressureFailures = 0;
    bool counterSaturated = false;
};

class MediaDatagramServiceShaper final {
public:
    static ::media::Result<std::unique_ptr<MediaDatagramServiceShaper>> create(
        MediaDatagramShapingPlan plan);
    ::media::Status rebind(MediaDatagramShapingPlan plan);
    ::media::Result<std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
    shape(MediaWireDatagramBatchBuffer& batch, MediaRunningTime now);
    const MediaDatagramShapingPlan& plan() const noexcept { return m_plan; }
    const MediaDatagramServiceShaperTelemetry& telemetry() const noexcept
    {
        return m_telemetry;
    }

private:
    struct PendingReservation final {
        std::uint64_t endpointId;
        std::uint64_t payloadBytes;
        std::uint64_t wireBytes;
        MediaRunningTime completion;
    };

    struct EndpointUsage final {
        std::uint64_t datagrams = 0;
        std::uint64_t bytes = 0;
    };

    struct PreparedReservation final {
        MediaDatagramPlannedWireCost cost;
        MediaRunningTime targetCompletion;
        MediaRunningTime endpointDeadline;
        MediaRunningTime backlogDeadline;
        MediaRunningTime enqueueNotAfter;
        std::uint64_t maximumPendingDatagrams;
        std::uint64_t maximumPendingBytes;
    };

    MediaDatagramServiceShaper(MediaDatagramShapingPlan plan,
                               MediaRunningTime burstDebtDuration) noexcept;

    MediaDatagramShapingPlan m_plan;
    MediaRunningTime m_burstDebtDuration;
    MediaDatagramServiceShaperTelemetry m_telemetry;
    std::vector<std::optional<PendingReservation>> m_pending;
    std::vector<PendingReservation> m_newPending;
    std::vector<PreparedReservation> m_preparedReservations;
    std::unordered_map<std::uint64_t, EndpointUsage> m_pendingByEndpoint;
    std::unordered_map<std::uint64_t, EndpointUsage> m_batchByEndpoint;
    std::unordered_map<std::uint64_t, EndpointUsage> m_expiredByEndpoint;
    std::size_t m_pendingHead = 0;
    std::size_t m_pendingCount = 0;
    std::uint64_t m_pendingDatagrams = 0;
    std::uint64_t m_pendingWireBytes = 0;
    std::optional<MediaRunningTime> m_physicalAvailable;
    std::optional<MediaRunningTime> m_pacingDebtUntil;
    std::optional<MediaRunningTime> m_previousCanonicalRelease;
    std::optional<MediaRunningTime> m_previousCanonicalDeadline;
    std::optional<std::uint64_t> m_previousGlobalSequence;
    std::optional<MediaRunningTime> m_previousNow;
};

} // namespace media::ffmpeg::graph
