#pragma once

#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"

namespace media::ffmpeg::graph {

// The lifecycle owner must keep the node set alive for this observation.
// Evidence records sender submission, not receiver delivery or audio readiness.
inline std::optional<MediaVideoOutputReadyEvidence> collectVideoOutputReadyEvidence(
    const MediaGraphScheduler& scheduler,
    const MediaGraphExecutionContext& context)
{
    for (const auto* node : scheduler.orderedRuntimeNodes(context)) {
        const auto* sender = dynamic_cast<const MediaScheduledDatagramSenderNode*>(node);
        if (sender) {
            if (auto evidence = sender->videoReadyEvidence()) return evidence;
        }
    }
    return std::nullopt;
}

} // namespace media::ffmpeg::graph
