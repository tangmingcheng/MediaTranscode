#pragma once

#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"

#include <media_transcode/Result.h>

#include <memory>

namespace media::ffmpeg::graph {

class MediaGraphExecutionContext;
class MediaGraphRuntimeCompiler;

struct MediaAvSyncClockBundle final {
    std::shared_ptr<MediaMasterClock> masterClock;
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
private:
    friend class MediaGraphRuntimeCompiler;
    static ::media::Result<MediaPlaybackEpochActivationCapability>
    registerGroupAndIssueActivationCapability(
        const MediaAvSyncRuntimeBinding& binding,
        MediaAvSyncClockBundle clocks,
        MediaGraphExecutionContext& context);
    MediaAvSyncRuntimeBootstrap() = delete;
};

} // namespace media::ffmpeg::graph
