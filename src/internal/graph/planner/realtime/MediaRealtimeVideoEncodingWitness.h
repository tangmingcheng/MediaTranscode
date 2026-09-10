#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeVideoEncodingGroupContract.h"
#include "internal/graph/builder/MediaEncodedBranchEndpoints.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include <memory>

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoEncodingWitness final {
    MediaPipelinePlan pipeline;
    MediaRealtimeVideoEncodingGroupContract actual;
    ::media::ffmpeg::BufferRefPtr sourceFramesOwner;
    MediaVideoJoinPlan joinPlan;
};

struct MediaRealtimeExistingVideoEncodingGroup final {
    std::uint64_t groupId;
    std::shared_ptr<const MediaRealtimeVideoEncodingWitness> witness;
    MediaEncodedBranchEndpoints encoded;
};

} // namespace media::ffmpeg::graph
