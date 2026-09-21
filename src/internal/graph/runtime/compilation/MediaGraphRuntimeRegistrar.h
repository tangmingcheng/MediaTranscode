#pragma once

#include "internal/graph/runtime/compilation/MediaAvRuntimeDomainState.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"

namespace media::ffmpeg::graph {

class MediaProtocolOutputRuntimeAuthority;

class MediaGraphRuntimeRegistrar final {
public:
    static ::media::Status registerDefaults(
        MediaGraphExecutionContext& context,
        MediaGraphScheduler& scheduler,
        std::vector<MediaPreparedRealtimeInputBinding>& inputBindings,
        std::vector<MediaAvRuntimeDomainState>& avDomains,
        const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>& protocolOutputAuthority);
};

} // namespace media::ffmpeg::graph
