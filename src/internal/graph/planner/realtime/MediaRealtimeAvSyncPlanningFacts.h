#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncPlanningFacts final {
    std::optional<int> outputSampleRate;
    std::optional<std::int64_t> decoderDelaySamples;
    std::optional<std::int64_t> encoderLookaheadSamples;
    std::optional<std::int64_t> decodeQueueSamples;
    std::optional<std::int64_t> resampleQueueSamples;
    std::optional<std::int64_t> encodeQueueSamples;
    std::optional<std::int64_t> schedulerQueueSamples;
    std::optional<std::int64_t> protocolBatchSamples;
    std::optional<std::int64_t> mailboxDeliveryMarginSamples;
    std::optional<std::int64_t> maximumResamplerOutputBlockSamples;
    std::optional<std::size_t> mailboxCapacity;
    std::optional<MediaRunningTime> acknowledgementTimeout;
    std::optional<MediaRunningTime> terminalDrainWindow;
    friend bool operator==(const MediaRealtimeAvSyncPlanningFacts&,
                           const MediaRealtimeAvSyncPlanningFacts&) = default;
};

struct MediaRealtimeAvSyncComponentBounds final {
    std::int64_t decoderDelaySamples;
    std::int64_t decodeQueueSamples;
    std::int64_t resampleQueueSamples;
    std::int64_t encodeQueueSamples;
    std::int64_t schedulerQueueSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::size_t mailboxCapacity;
    friend bool operator==(const MediaRealtimeAvSyncComponentBounds&,
                           const MediaRealtimeAvSyncComponentBounds&) = default;
};

} // namespace media::ffmpeg::graph
