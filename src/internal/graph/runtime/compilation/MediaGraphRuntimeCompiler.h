#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"

#include <media_transcode/Result.h>

#include <atomic>
#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphRuntimeState;

class MediaGraphRuntimeCompiler final {
public:
    static ::media::Status compile(
        MediaRealtimeExecutableGraph executable,
        MediaGraph& activeGraph,
        std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
        MediaGraphExecutionContext& context,
        MediaGraphScheduler& scheduler,
        MediaGraphThreadedExecutor& threadedExecutor,
        MediaRuntimeAcceptanceCollector& acceptanceCollector,
        std::atomic_size_t& queueHighWatermark,
        MediaGraphRuntimeState& state);

    static ::media::Status validateBindings(const MediaRealtimeExecutableGraph& executable);
    static ::media::Status registerNode(MediaGraphScheduler& scheduler, std::unique_ptr<MediaRuntimeNode> node);
    static ::media::Status registerDefaults(
        MediaGraphExecutionContext& context,
        MediaGraphScheduler& scheduler,
        std::vector<MediaPreparedRealtimeInputBinding>& inputBindings);

private:
    MediaGraphRuntimeCompiler() = delete;
};

} // namespace media::ffmpeg::graph
