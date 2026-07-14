#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaAudioCorrectionBuffer::MediaAudioCorrectionBuffer(
    MediaAudioCompensationCommand command)
    : m_command(std::move(command))
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("audio correction");
}

MediaBufferType MediaAudioCorrectionBuffer::type() const noexcept
{
    return MediaBufferType::Control;
}

const MediaAudioCompensationCommand& MediaAudioCorrectionBuffer::command() const noexcept
{
    return m_command;
}

} // namespace media::ffmpeg::graph
