#include "internal/graph/runtime/qos/MediaQosController.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaQosController::setAdaptiveBitrateController(MediaAdaptiveBitrateController controller)
{
    m_abr = std::move(controller);
}

MediaQosAction MediaQosController::update(const MediaGraphRuntimeReport& report,
                                           const MediaAdaptiveBitrateSample& sample)
{
    MediaQosAction action;

    const MediaBitrateLadderEntry* before = m_abr.current();
    const MediaBitrateLadderEntry* after = m_abr.update(sample);

    if (!report.backpressure.healthy()) {
        action.kind = MediaQosActionKind::DropLateFrames;
        action.targetBitrate = after;
        return action;
    }

    if (before && after && after->bitrate < before->bitrate) {
        action.kind = MediaQosActionKind::ReduceBitrate;
        action.targetBitrate = after;
        return action;
    }

    if (before && after && after->bitrate > before->bitrate) {
        action.kind = MediaQosActionKind::IncreaseBitrate;
        action.targetBitrate = after;
        return action;
    }

    action.kind = MediaQosActionKind::Keep;
    action.targetBitrate = after;
    return action;
}

} // namespace media::ffmpeg::graph
