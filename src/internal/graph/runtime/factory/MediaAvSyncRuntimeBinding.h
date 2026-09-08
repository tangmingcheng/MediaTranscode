#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <cstdint>
#include <memory>
#include <variant>

namespace media::ffmpeg::graph {

class MediaAvStartupVideoPreparationState;

struct MediaAvSyncComponentCoreRuntimeProduct final {};

enum class MediaSynchronizedAudioExecutionProduct : std::uint8_t {
    PacketCopy = 0,
    FrameTranscode = 1
};

using MediaAvSyncRuntimeOutputProduct = std::variant<
    MediaAvSyncComponentCoreRuntimeProduct,
    MediaSeparateRtpOutputRuntimePlan,
    MediaProjectMpegTsRuntimeOutputPlan>;

struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaAvGenerationTransitionPlan transition;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaDatagramTransportPlanTemplate datagramTransport;
    MediaSynchronizedAudioExecutionProduct audioExecutionProduct;
    MediaAvSyncRuntimeOutputProduct outputProduct;
    std::shared_ptr<MediaAvStartupVideoPreparationState>
        videoPreparationState;
};

} // namespace media::ffmpeg::graph
