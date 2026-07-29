#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaAvSyncPlan;

class MediaAvSyncSharedNtpEpochRequirement final {
public:
    static ::media::Result<bool> resolve(const MediaAvSyncPlan& plan);

private:
    MediaAvSyncSharedNtpEpochRequirement() = delete;
};

} // namespace media::ffmpeg::graph
