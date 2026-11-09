#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeGraphResourceLedgerPlan;
struct MediaHardwareDescriptor;
struct MediaPreparedInputRetentionStream;
struct MediaRealtimeDeploymentLatencyBudget;

struct MediaRealtimeMediaCapacityPlan final {
    std::size_t videoUnits;
    std::uint64_t videoUnitBytes;
    std::uint64_t videoBytes;
    std::optional<std::size_t> audioUnits;
    std::optional<std::uint64_t> audioUnitBytes;
    std::optional<std::uint64_t> audioBytes;
    MediaRunningTime maximumGap;
    friend bool operator==(const MediaRealtimeMediaCapacityPlan&, const MediaRealtimeMediaCapacityPlan&) = default;
};

class MediaRealtimeMediaCapacityPlanner final {
public:
    static ::media::Result<MediaRealtimeMediaCapacityPlan> plan(
        const MediaRealtimeGraphResourceLedgerPlan& ledger);

    // Decoded startup retains logical frame payload, not compressed access units.
    // Units are a finite admission count, not a proof of a buffered time span.
    static ::media::Result<MediaRealtimeMediaCapacityPlan> planDecodedSource(
        const MediaHardwareDescriptor& decoderOutput, std::size_t videoUnits,
        const MediaPreparedInputRetentionStream* audio,
        const MediaRealtimeDeploymentLatencyBudget& latency);
private:
    MediaRealtimeMediaCapacityPlanner() = delete;
};

} // namespace media::ffmpeg::graph
