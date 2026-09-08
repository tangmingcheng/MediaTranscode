#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaFinalGraphResourceScope : std::uint8_t {
    EngineManagedPayloadAndReservedStorage = 1,
    ObservedOnlyExternalAllocation = 2,
    AccountedByNetworkLedger = 3,
    ObservedOnlyDeviceAndDriverAllocation = 4
};

struct MediaFinalGraphResourceLedgerEntry final {
    std::string owner;
    std::string sharedAllocationGroup;
    MediaFinalGraphResourceScope scope;
    std::uint64_t payloadBytes;
    std::uint64_t queueSlots;
    std::uint64_t maximumBufferObjects;
    std::uint64_t retainedItems;
    bool coveredByGlobalPayloadLedger;
    std::string authority;
};

struct MediaEncoderHardwareFramesPoolPlan final {
    std::uint64_t initialPoolSurfaces;
    std::uint64_t graphInFlightSurfaces;
    std::uint64_t pipelinePendingSurfaces;
    std::uint64_t encoderRetainedSurfaces;
    std::string authority;
};

struct MediaFinalGraphResourceLedger final {
    MediaRealtimeGraphResourceBudgetScope resourceScope;
    std::uint64_t maximumGraphPayloadAndReservedStorageBytes;
    std::uint64_t admittedGraphPayloadAndReservedStorageBytes;
    std::uint64_t admittedDeviceAndDriverBytes;
    std::vector<MediaFinalGraphResourceLedgerEntry> entries;
    std::vector<std::string> outOfScopeAuthorities;
    std::optional<MediaEncoderHardwareFramesPoolPlan> encoderFramesPool;
    MediaGraphPayloadCreditPlan payloadCreditPlan;
};

class MediaFinalGraphResourceLedgerCompiler final {
public:
    static ::media::Result<MediaFinalGraphResourceLedger> compile(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& planningLedger);
};

} // namespace media::ffmpeg::graph
