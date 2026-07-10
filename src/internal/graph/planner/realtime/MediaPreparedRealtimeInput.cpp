#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaPreparedRealtimeInput::MediaPreparedRealtimeInput(
    std::unique_ptr<FFmpegFormatContextBuffer> buffer)
    : m_buffer(std::move(buffer))
{
}

::media::Result<MediaPreparedRealtimeInput> MediaPreparedRealtimeInput::create(
    ::media::ffmpeg::InputFormatContextPtr context)
{
    auto buffer = FFmpegFormatContextBuffer::createInput(std::move(context));
    if (!buffer) {
        return ::media::Result<MediaPreparedRealtimeInput>::failure(buffer.error());
    }
    return ::media::Result<MediaPreparedRealtimeInput>::success(
        MediaPreparedRealtimeInput(std::move(buffer).value()));
}

bool MediaPreparedRealtimeInput::valid() const noexcept
{
    return m_buffer && m_buffer->context() && m_buffer->inputSnapshotComplete();
}

AVFormatContext* MediaPreparedRealtimeInput::context() noexcept
{
    return m_buffer ? m_buffer->context() : nullptr;
}

const AVFormatContext* MediaPreparedRealtimeInput::context() const noexcept
{
    return m_buffer ? m_buffer->context() : nullptr;
}

const FFmpegInputStreamSnapshot* MediaPreparedRealtimeInput::inputStreamSnapshot(int streamIndex) const noexcept
{
    return m_buffer ? m_buffer->inputStreamSnapshot(streamIndex) : nullptr;
}

MediaBufferRef MediaPreparedRealtimeInput::releaseFormatBuffer() noexcept
{
    return MediaBufferRef(m_buffer.release());
}

} // namespace media::ffmpeg::graph
