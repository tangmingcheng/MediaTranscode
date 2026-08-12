#include "internal/graph/runtime/buffer/MediaPreparedGenericInputBuffer.h"

namespace media::ffmpeg::graph {

MediaPreparedGenericInputBuffer::MediaPreparedGenericInputBuffer(
    MediaPreparedGenericInput input)
    : m_input(std::move(input))
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

MediaBufferType MediaPreparedGenericInputBuffer::type() const noexcept
{
    return MediaBufferType::FormatContext;
}

const FFmpegInputStreamSnapshot*
MediaPreparedGenericInputBuffer::inputStreamSnapshot(int streamIndex) const noexcept
{
    const auto& snapshots = m_input.snapshots();
    if (streamIndex < 0 || static_cast<std::size_t>(streamIndex) >= snapshots.size() ||
        snapshots[static_cast<std::size_t>(streamIndex)].index != streamIndex) {
        return nullptr;
    }
    return &snapshots[static_cast<std::size_t>(streamIndex)];
}

bool MediaPreparedGenericInputBuffer::inputSnapshotComplete() const noexcept
{
    return !m_input.snapshots().empty();
}

::media::Result<MediaDemuxInputSession>
MediaPreparedGenericInputBuffer::takeDemuxSession()
{
    if (m_transferred) {
        return ::media::Result<MediaDemuxInputSession>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared generic input session was already transferred"));
    }
    m_transferred = true;
    return m_input.takeSession();
}

const MediaPreparedGenericInputPlan&
MediaPreparedGenericInputBuffer::plan() const noexcept
{
    return m_input.plan();
}

const MediaPreparedGenericInputEvidence&
MediaPreparedGenericInputBuffer::evidence() const noexcept
{
    return m_input.evidence();
}

const MediaAvSyncStartupPolicy&
MediaPreparedGenericInputBuffer::startup() const noexcept
{
    return m_input.startup();
}

} // namespace media::ffmpeg::graph
