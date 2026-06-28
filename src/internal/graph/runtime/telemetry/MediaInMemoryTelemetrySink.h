#pragma once

#include "internal/graph/runtime/telemetry/MediaTelemetrySink.h"

#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class MediaInMemoryTelemetrySink final : public MediaTelemetrySink {
public:
    void record(const MediaTraceEvent& event) override;
    std::vector<MediaTraceEvent> snapshot() const;
    void clear();

private:
    mutable std::mutex m_mutex;
    std::vector<MediaTraceEvent> m_events;
};

} // namespace media::ffmpeg::graph
