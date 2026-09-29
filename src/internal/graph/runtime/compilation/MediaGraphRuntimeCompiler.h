#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/compilation/MediaAvRuntimeDomainState.h"

#include <media_transcode/Result.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphRuntimeState;
class MediaGraphRuntime;
class MediaAvSyncClockSource;
class MediaAvStartupVideoPreparationState;
class MediaProtocolOutputRuntimeAuthority;

class MediaGraphRuntimeCompiler final {
public:
    static ::media::Status validateBindings(const MediaRealtimeExecutableGraph& executable);

private:
    friend class MediaGraphRuntime;

    static ::media::Status compile(
        MediaRealtimeExecutableGraph executable,
        MediaGraph& activeGraph,
        std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
        std::vector<MediaAvRuntimeDomainState>& avDomains,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
            protocolOutputAuthority,
        const std::shared_ptr<MediaAvSyncClockSource>& avSyncClockSource,
        MediaGraphExecutionContext& context,
        MediaGraphScheduler& scheduler,
        MediaGraphThreadedExecutor& threadedExecutor,
        MediaRuntimeAcceptanceCollector& acceptanceCollector,
        std::atomic_size_t& queueHighWatermark,
        MediaGraphRuntimeState& state);

    static ::media::Status registerNode(MediaGraphScheduler& scheduler, std::unique_ptr<MediaRuntimeNode> node);
    MediaGraphRuntimeCompiler() = delete;
};

} // namespace media::ffmpeg::graph
