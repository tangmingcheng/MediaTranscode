#pragma once

#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/planner/capability/MediaVideoEncoderPreparer.h"
#include "internal/graph/planner/realtime/MediaVideoOutputPreparationPlan.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"

namespace media::ffmpeg::graph {

struct MediaProbedVideoOutput final {
    MediaVideoOutputPlan plan;
    ::media::ffmpeg::BufferRefPtr device;
    MediaVideoEncoderFrameInput frameInput;
    MediaVideoCanvasAllocation allocation;
};

// Candidate probing and final opening share one device, not a compressed source.
class MediaVideoOutputPreparer final {
public:
    static ::media::Result<::media::ffmpeg::BufferRefPtr> openDevice(
        const MediaPipelineStagePlan& encoder, const MediaPreparationControl& control);
    static ::media::Result<MediaProbedVideoOutput> probe(
        MediaPipelineStagePlan encoder, MediaEncoderOpenContract open,
        MediaVideoEncoderFrameInput frameInput, ::media::ffmpeg::BufferRefPtr device,
        const MediaPreparationControl& control);
    static ::media::Result<MediaPreparedVideoEncoder> prepare(
        const MediaProbedVideoOutput& probed, const MediaVideoOutputPreparationPlan& plan,
        const std::shared_ptr<MediaPreparationStorageBudget>& storage,
        const MediaPreparationControl& control);
};

} // namespace media::ffmpeg::graph
