#include "internal/graph/runtime/telemetry/MediaInMemoryTelemetrySink.h"

namespace media::ffmpeg::graph {

void MediaInMemoryTelemetrySink::record(const MediaTraceEvent& event)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_events.push_back(event);
}

std::vector<MediaTraceEvent> MediaInMemoryTelemetrySink::snapshot() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_events;
}

void MediaInMemoryTelemetrySink::clear()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_events.clear();
}

} // namespace media::ffmpeg::graph
