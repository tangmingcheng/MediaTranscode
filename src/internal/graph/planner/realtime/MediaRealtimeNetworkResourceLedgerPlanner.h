#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/planner/realtime/MediaWireTrafficEnvelope.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRealtimeNetworkAccountingGroup : std::uint8_t {
    BacklogPayload = 1,
    BacklogContainer = 2,
    BatchContainer = 3,
    EndpointPendingContainer = 4,
    EvidenceCorrelation = 5,
    SocketKernelBuffer = 6
};

struct MediaRealtimeNetworkResourceLedgerEntry final {
    MediaRealtimeNetworkAccountingGroup group;
    std::uint64_t itemCount;
    std::uint64_t bytes;
    bool chargedToSocketBudget;
    std::string authority;
};

struct MediaRealtimeNetworkResourceLedgerPlan final {
    std::uint64_t maximumBacklogDatagrams;
    std::uint64_t maximumBacklogBytes;
    std::uint64_t maximumBatchDatagrams;
    std::uint64_t maximumBatchBytes;
    std::uint64_t maximumEndpointPendingDatagrams;
    std::uint64_t maximumEndpointPendingBytes;
    MediaDatagramSocketBufferPlan socketBufferPerEndpoint;
    std::uint64_t maximumCorrelationEntries;
    std::uint64_t admittedNetworkBytes;
    std::uint64_t admittedSocketBytes;
    std::vector<MediaRealtimeNetworkResourceLedgerEntry> entries;
};

class MediaRealtimeNetworkResourceLedgerPlanner final {
public:
    static ::media::Result<MediaRealtimeNetworkResourceLedgerPlan> plan(
        const MediaRealtimeDeploymentLatencyBudget& latency,
        const MediaRealtimeDeploymentObservationBudget& observation,
        const MediaWireTrafficEnvelope& wire,
        std::uint64_t endpointCount);
    static ::media::Status validate(
        const MediaRealtimeNetworkResourceLedgerPlan& ledger);

private:
    MediaRealtimeNetworkResourceLedgerPlanner() = delete;
};

} // namespace media::ffmpeg::graph
