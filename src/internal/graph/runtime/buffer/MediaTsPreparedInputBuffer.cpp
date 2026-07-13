#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"

namespace media::ffmpeg::graph {

MediaTsPreparedInputBuffer::MediaTsPreparedInputBuffer(
    std::unique_ptr<MediaTsDemuxSession> session,
    std::vector<FFmpegInputStreamSnapshot> streamSnapshots)
    : m_session(std::move(session)), m_streamSnapshots(std::move(streamSnapshots))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::FormatContext);
    if (m_session) {
        m_programSnapshots = m_session->programSnapshots();
        const auto inventory = m_session->programInventory();
        m_programInventory = inventory;
    }
}

const std::vector<FFmpegInputProgramSnapshot>&
MediaTsPreparedInputBuffer::programSnapshots() const noexcept
{
    return m_programSnapshots;
}

::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>
MediaTsPreparedInputBuffer::create(std::unique_ptr<MediaTsDemuxSession> session)
{
    if (!session) {
        return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS prepared buffer requires a session"));
    }
    auto snapshots = session->cloneStreamSnapshots();
    if (!snapshots) {
        return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::failure(
            snapshots.error());
    }
    return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::success(
        std::unique_ptr<MediaTsPreparedInputBuffer>(new MediaTsPreparedInputBuffer(
            std::move(session), std::move(snapshots.value()))));
}

MediaBufferType MediaTsPreparedInputBuffer::type() const noexcept
{
    return MediaBufferType::FormatContext;
}

const std::vector<FFmpegInputStreamSnapshot>&
MediaTsPreparedInputBuffer::streamSnapshots() const noexcept
{
    return m_streamSnapshots;
}

const MediaTsProgramInventorySnapshot&
MediaTsPreparedInputBuffer::programInventory() const noexcept
{
    return m_programInventory;
}

::media::Result<std::unique_ptr<MediaTsDemuxSession>>
MediaTsPreparedInputBuffer::takeSession()
{
    if (!m_session) {
        return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS prepared session was already transferred"));
    }
    return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::success(std::move(m_session));
}

} // namespace media::ffmpeg::graph
