#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaPreparedInputPayloadEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRealtimeResourceAccountingGroup : std::uint8_t {
    EncodedVideoPacket = 1,
    DecodedVideoSurface = 2,
    EncodedAudioPacket = 3,
    MuxDescriptor = 4,
    RetainLatestMetadata = 5,
    PreparedInputPacket = 6,
    PreparedInputReservedStorage = 7
};

enum class MediaRealtimeQueueRetentionSemantics : std::uint8_t {
    BoundedFifo = 1,
    RetainLatest = 2
};

struct MediaRealtimeGraphResourceLedgerEntry final {
    MediaRealtimeResourceAccountingGroup accountingGroup;
    MediaRealtimeQueueRetentionSemantics retention;
    std::uint64_t itemCount;
    std::uint64_t payloadBytes;
    std::string authority;
};

struct MediaRealtimeGraphResourceLedgerPlan final {
    MediaGraphQueueParameters queues;
    MediaRealtimeMediaCapacityPlan media;
    MediaRealtimeGraphResourceBudgetScope resourceScope;
    std::uint64_t maximumGraphPayloadAndReservedStorageBytes;
    std::uint64_t videoSurfaceUnitBytes;
    std::optional<std::uint64_t> audioFrameUnitBytes;
    std::optional<MediaPreparedHardwareMemoryEnvelope> hardwareMemory;
    std::uint64_t maximumEncoderRetainedFrames;
    bool hardwareEncoderSurfacePool;
    std::optional<MediaPreparedInputPayloadEnvelope> preparedInputPayload;
    std::vector<MediaRealtimeGraphResourceLedgerEntry> entries;
};

struct MediaRealtimeVideoSurfaceFootprintFact final {
    int width = 0;
    int height = 0;
    std::string pixelFormat;
    MediaRational productionRate;
    std::string authority;
};

class MediaRealtimeGraphResourceLedgerPlanner final {
public:
    static ::media::Result<MediaRealtimeGraphResourceLedgerPlan> plan(
        MediaRealtimeDeploymentLatencyBudget latency,
        const MediaPreparedRealtimeEmissionSet& emission,
        const std::vector<MediaRealtimeVideoSurfaceFootprintFact>& videoSurfaces,
        bool hardwareSurface);
    static ::media::Result<MediaRealtimeGraphResourceLedgerPlan>
    admitPreparedInput(
        MediaRealtimeGraphResourceLedgerPlan ledger,
        MediaPreparedInputPayloadEnvelope payload,
        std::uint64_t reservedStorageBytes,
        std::string reservedStorageAuthority);
    static ::media::Status validate(
        const MediaRealtimeGraphResourceLedgerPlan& ledger);

private:
    MediaRealtimeGraphResourceLedgerPlanner() = delete;
};

} // namespace media::ffmpeg::graph
