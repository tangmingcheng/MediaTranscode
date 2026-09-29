#pragma once

#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"

namespace media::ffmpeg::graph {

class MediaDemuxClockRuntimeAssembler final {
public:
    static ::media::Result<std::vector<std::unique_ptr<MediaRuntimeNode>>> create(
        MediaGraphExecutionContext& context,
        const MediaAvSyncGroupKey& groupKey,
        const MediaAvDemuxClockRegistration& registration);
};

} // namespace media::ffmpeg::graph
