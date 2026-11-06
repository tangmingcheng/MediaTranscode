#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlanView.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncPlanValidator final {
public:
    static ::media::Status validateSourceClock(const MediaAvSyncPlanView& plan);
    static ::media::Status validate(const MediaAvSyncPlanView& plan);
    static ::media::Status validatePolicy(const MediaAvSyncPlanView& plan);
    static ::media::Status validateRuntime(const MediaAvSyncPlanView& plan);
    static ::media::Status validateDomain(
        const MediaAvSyncPlanView& plan, MediaAvSyncDomainRole expectedRole);

private:
    MediaAvSyncPlanValidator() = delete;
};

} // namespace media::ffmpeg::graph
