#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeDeploymentServiceScope final {
    MediaDatagramServiceScopeKind kind = MediaDatagramServiceScopeKind::Unknown;
    std::string scopeId;
    std::string coverageAuthority;
    friend bool operator==(const MediaRealtimeDeploymentServiceScope&,
                           const MediaRealtimeDeploymentServiceScope&) = default;
};

struct MediaRealtimeDeploymentResourceBudget final {
    std::uint64_t maximumBacklogDatagrams = 0;
    std::uint64_t maximumBacklogBytes = 0;
    MediaRunningTime maximumResidence = MediaRunningTime::fromNanoseconds(0);
    std::uint64_t maximumBatchDatagrams = 0;
    std::uint64_t maximumBatchBytes = 0;
    std::uint64_t maximumEndpointPendingDatagrams = 0;
    std::uint64_t maximumEndpointPendingBytes = 0;
    std::uint64_t socketHardBoundBytes = 0;
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentResourceBudget&,
                           const MediaRealtimeDeploymentResourceBudget&) = default;
};

struct MediaRealtimeDeploymentLatencyBudget final {
    MediaRunningTime targetResidence = MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime maximumResidence = MediaRunningTime::fromNanoseconds(0);
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentLatencyBudget&,
                           const MediaRealtimeDeploymentLatencyBudget&) = default;
};

struct MediaRealtimeDeploymentObservationBudget final {
    std::uint64_t maximumRunDatagrams = 0;
    std::uint64_t maximumCorrelationEntries = 0;
    MediaRunningTime maximumDrainResidence = MediaRunningTime::fromNanoseconds(0);
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentObservationBudget&,
                           const MediaRealtimeDeploymentObservationBudget&) = default;
};

struct MediaRealtimeDeploymentEnvelopeEncoding final {
    MediaRealtimeDeploymentServiceScope serviceScope;
    MediaDatagramMtuEvidence mtu;
    MediaDatagramServiceCurvePlan service;
    MediaRealtimeDeploymentResourceBudget resources;
    MediaRealtimeDeploymentLatencyBudget latency;
    MediaRealtimeDeploymentObservationBudget observation;
    friend bool operator==(const MediaRealtimeDeploymentEnvelopeEncoding&,
                           const MediaRealtimeDeploymentEnvelopeEncoding&) = default;
};

class MediaRealtimeDeploymentEnvelope final {
public:
    static ::media::Result<MediaRealtimeDeploymentEnvelope> decode(
        MediaRealtimeDeploymentEnvelopeEncoding encoding);

    const MediaRealtimeDeploymentEnvelopeEncoding& encode() const noexcept
    {
        return m_encoding;
    }

private:
    explicit MediaRealtimeDeploymentEnvelope(
        MediaRealtimeDeploymentEnvelopeEncoding encoding) noexcept;
    MediaRealtimeDeploymentEnvelopeEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
