#pragma once

#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"

#include <media_transcode/Result.h>

#include <memory>

namespace media::ffmpeg::graph {

class MediaGraphExecutionContext;

struct MediaAvSyncClockBundle final {
    std::shared_ptr<MediaSteadyMasterClock> masterClock;
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch;
};

class MediaAvSyncClockSource {
public:
    virtual ~MediaAvSyncClockSource() = default;
    virtual ::media::Result<MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) = 0;
};

class MediaAvSyncRuntimeBootstrap final {
public:
    static ::media::Result<MediaAvSyncClockBundle> createClocks(
        const MediaAvSyncRuntimeBinding& binding,
        MediaAvSyncClockSource& source);
    static ::media::Status registerGroup(
        const MediaAvSyncRuntimeBinding& binding,
        MediaAvSyncClockBundle clocks,
        MediaGraphExecutionContext& context);

private:
    MediaAvSyncRuntimeBootstrap() = delete;
};

} // namespace media::ffmpeg::graph
