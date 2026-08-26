#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRealtimeResourceAccountingGroup : std::uint8_t {
    EncodedVideoPacket = 1,
    DecodedVideoSurface = 2,
    EncodedAudioPacket = 3,
    MuxDescriptor = 4,
    RetainLatestMetadata = 5
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
    std::optional<MediaPreparedHardwareMemoryEnvelope> hardwareMemory;
    std::uint64_t maximumEncoderRetainedFrames;
    bool hardwareEncoderSurfacePool;
    std::vector<MediaRealtimeGraphResourceLedgerEntry> entries;
};

class MediaRealtimeGraphResourceLedgerPlanner final {
public:
    static ::media::Result<MediaRealtimeGraphResourceLedgerPlan> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaPreparedRealtimeEmissionSet& emission,
        int outputWidth,
        int outputHeight,
        std::string_view surfacePixelFormat,
        bool hardwareSurface);
    static ::media::Status validate(
        const MediaRealtimeGraphResourceLedgerPlan& ledger);

private:
    MediaRealtimeGraphResourceLedgerPlanner() = delete;
};

} // namespace media::ffmpeg::graph
