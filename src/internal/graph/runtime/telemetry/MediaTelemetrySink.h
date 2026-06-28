#pragma once

#include "internal/graph/runtime/telemetry/MediaTraceEvent.h"

namespace media::ffmpeg::graph {

class MediaTelemetrySink {
public:
    virtual ~MediaTelemetrySink() = default;
    virtual void record(const MediaTraceEvent& event) = 0;

protected:
    MediaTelemetrySink() = default;
};

} // namespace media::ffmpeg::graph
