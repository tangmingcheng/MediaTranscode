#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"

namespace media::ffmpeg::graph {

MediaTsPreparedInputBuffer::MediaTsPreparedInputBuffer(
    std::vector<FFmpegInputStreamSnapshot> streamSnapshots,
    std::vector<FFmpegInputProgramSnapshot> programSnapshots,
    MediaTsProgramInventorySnapshot programInventory,
    MediaTsRuntimeSessionFactory runtimeSessionFactory)
    : m_runtimeSessionFactory(std::move(runtimeSessionFactory)),
      m_streamSnapshots(std::move(streamSnapshots)),
      m_programSnapshots(std::move(programSnapshots)),
      m_programInventory(std::move(programInventory))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::FormatContext);
}

const std::vector<FFmpegInputProgramSnapshot>&
MediaTsPreparedInputBuffer::programSnapshots() const noexcept
{
    return m_programSnapshots;
}

::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>
MediaTsPreparedInputBuffer::create(
    std::unique_ptr<MediaTsDemuxSession> preflightSession,
    MediaTsRuntimeSessionFactory runtimeSessionFactory)
{
    if (!preflightSession || !runtimeSessionFactory) {
        return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS prepared input requires preflight and runtime sessions"));
    }
    auto snapshots = preflightSession->cloneStreamSnapshots();
    if (!snapshots) {
        return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::failure(
            snapshots.error());
    }
    auto programs = preflightSession->programSnapshots();
    auto inventory = preflightSession->programInventory();
    if (auto closed = preflightSession->close(); !closed) {
        return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::failure(
            closed.error());
    }
    return ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>>::success(
        std::unique_ptr<MediaTsPreparedInputBuffer>(new MediaTsPreparedInputBuffer(
            std::move(snapshots).value(), std::move(programs), std::move(inventory),
            std::move(runtimeSessionFactory))));
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

const FFmpegInputStreamSnapshot* MediaTsPreparedInputBuffer::inputStreamSnapshot(
    int streamIndex) const noexcept
{
    for (const auto& stream : m_streamSnapshots) {
        if (stream.index == streamIndex) return &stream;
    }
    return nullptr;
}

const MediaTsProgramInventorySnapshot&
MediaTsPreparedInputBuffer::programInventory() const noexcept
{
    return m_programInventory;
}

::media::Result<std::unique_ptr<MediaTsDemuxSession>>
MediaTsPreparedInputBuffer::takeSession()
{
    if (m_session) {
        return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::success(
            std::move(m_session));
    }
    return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::failure(
        ::media::ErrorInfo::notInitialized("MPEG-TS prepared session was already transferred"));
}

::media::Status MediaTsPreparedInputBuffer::materializeSession()
{
    if (m_session) return ::media::Status::success();
    if (!m_runtimeSessionFactory) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MPEG-TS runtime session factory was already consumed"));
    }
    auto factory = std::move(m_runtimeSessionFactory);
    m_runtimeSessionFactory = {};
    auto session = factory();
    if (!session) return ::media::Status::failure(session.error());
    m_session = std::move(session).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
