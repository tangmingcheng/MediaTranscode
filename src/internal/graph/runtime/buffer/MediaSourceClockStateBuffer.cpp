#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"

namespace media::ffmpeg::graph {

MediaSourceClockStateBuffer::MediaSourceClockStateBuffer(
    MediaSourceClockReadiness readiness,
    std::uint64_t generation,
    bool discontinuity,
    std::optional<std::uint64_t> evidenceRevision)
    : m_readiness(readiness)
    , m_generation(generation)
    , m_evidenceRevision(evidenceRevision)
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
