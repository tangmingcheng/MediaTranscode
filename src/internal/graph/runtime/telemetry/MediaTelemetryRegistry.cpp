#include "internal/graph/runtime/telemetry/MediaTelemetryRegistry.h"

namespace media::ffmpeg::graph {

void MediaTelemetryRegistry::addSink(std::shared_ptr<MediaTelemetrySink> sink)
{
    if (!sink) {
        return;
    }

    std::lock_guard<std::mutex> locker(m_mutex);
    m_sinks.push_back(std::move(sink));
}

void MediaTelemetryRegistry::clearSinks()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_sinks.clear();
}

void MediaTelemetryRegistry::emit(MediaTraceEvent event)
{
    event.sequence = m_sequence.fetch_add(1, std::memory_order_relaxed);
    event.timestamp = std::chrono::steady_clock::now();

    std::vector<std::shared_ptr<MediaTelemetrySink>> sinks;
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        sinks = m_sinks;
    }

    for (const auto& sink : sinks) {
        if (sink) {
            sink->record(event);
        }
    }
}

} // namespace media::ffmpeg::graph
