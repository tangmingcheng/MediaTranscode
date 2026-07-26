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
class MediaAvSyncGroupRuntime;

struct MediaAvSyncClockBundle final {
    std::shared_ptr<MediaMasterClock> masterClock;
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch;
};

struct MediaAvReacquisitionAssemblyDependencies final {
    std::shared_ptr<MediaAvEpochTransitionService> transitionService;
    std::shared_ptr<MediaMasterClock> masterClock;
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
    static ::media::Result<MediaAvReacquisitionAssemblyDependencies>
    reacquisitionAssemblyDependencies(
        const MediaPlaybackEpochActivationCapability& capability,
        const std::shared_ptr<MediaAvSyncGroupRuntime>& group);
    MediaAvSyncRuntimeBootstrap() = delete;
};

} // namespace media::ffmpeg::graph
