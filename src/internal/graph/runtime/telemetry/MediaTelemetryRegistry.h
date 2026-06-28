#pragma once

#include "internal/graph/runtime/telemetry/MediaTelemetrySink.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTelemetryRegistry final {
public:
    void addSink(std::shared_ptr<MediaTelemetrySink> sink);
    void clearSinks();
    void emit(MediaTraceEvent event);

private:
    std::atomic_uint64_t m_sequence{ 1 };
    std::mutex m_mutex;
    std::vector<std::shared_ptr<MediaTelemetrySink>> m_sinks;
};

} // namespace media::ffmpeg::graph
