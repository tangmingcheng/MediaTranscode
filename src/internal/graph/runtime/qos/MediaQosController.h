#pragma once

#include "internal/graph/runtime/MediaBackpressureController.h"
#include "internal/graph/runtime/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/streaming/MediaAdaptiveBitrateController.h"

namespace media::ffmpeg::graph {

enum class MediaQosActionKind {
    None,
    Keep,
    ReduceBitrate,
    IncreaseBitrate,
    DropLateFrames,
    RequestKeyFrame
};

struct MediaQosAction {
    MediaQosActionKind kind = MediaQosActionKind::None;
    const MediaBitrateLadderEntry* targetBitrate = nullptr;
};

class MediaQosController final {
public:
    void setAdaptiveBitrateController(MediaAdaptiveBitrateController controller);
    MediaQosAction update(const MediaGraphRuntimeReport& report,
                          const MediaAdaptiveBitrateSample& sample);

private:
    MediaAdaptiveBitrateController m_abr;
};

} // namespace media::ffmpeg::graph
