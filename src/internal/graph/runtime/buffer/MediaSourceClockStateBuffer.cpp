#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"

namespace media::ffmpeg::graph {

MediaSourceClockStateBuffer::MediaSourceClockStateBuffer(
    MediaSourceClockReadiness readiness,
    std::uint64_t generation,
    bool discontinuity)
    : m_readiness(readiness)
    , m_generation(generation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    if (discontinuity) addFlags(MediaBufferFlag::Discontinuity);
    setDiagnosticName("source.clock_state");
}

MediaBufferType MediaSourceClockStateBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

MediaSourceClockReadiness MediaSourceClockStateBuffer::readiness() const noexcept
{
    return m_readiness;
}

std::uint64_t MediaSourceClockStateBuffer::generation() const noexcept
{
    return m_generation;
}

} // namespace media::ffmpeg::graph
