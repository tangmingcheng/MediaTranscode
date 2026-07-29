#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

MediaPreparedRealtimeInput::MediaPreparedRealtimeInput(
    std::unique_ptr<FFmpegFormatContextBuffer> buffer)
    : m_genericBuffer(std::move(buffer))
{
}

MediaPreparedRealtimeInput::MediaPreparedRealtimeInput(
    std::unique_ptr<MediaTsPreparedInputBuffer> buffer)
    : m_tsBuffer(std::move(buffer))
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

::media::Result<MediaPreparedRealtimeInput> MediaPreparedRealtimeInput::createMpegTs(
    std::unique_ptr<MediaTsInputSession> session)
{
    auto buffer = MediaTsPreparedInputBuffer::create(std::move(session));
    if (!buffer) return ::media::Result<MediaPreparedRealtimeInput>::failure(buffer.error());
    return ::media::Result<MediaPreparedRealtimeInput>::success(
        MediaPreparedRealtimeInput(std::move(buffer.value())));
}

::media::Result<MediaPreparedRealtimeInput>
MediaPreparedRealtimeInput::createDeferredMpegTs(
    std::unique_ptr<MediaTsInputSession> preflightSession,
    MediaTsRuntimeSessionFactory runtimeSessionFactory)
{
    auto buffer = MediaTsPreparedInputBuffer::createDeferred(
        std::move(preflightSession), std::move(runtimeSessionFactory));
    if (!buffer) {
        return ::media::Result<MediaPreparedRealtimeInput>::failure(buffer.error());
    }
    return ::media::Result<MediaPreparedRealtimeInput>::success(
        MediaPreparedRealtimeInput(std::move(buffer).value()));
}

bool MediaPreparedRealtimeInput::valid() const noexcept
{
    return (m_genericBuffer && m_genericBuffer->context() &&
            m_genericBuffer->inputSnapshotComplete()) || m_tsBuffer;
}

std::optional<MediaPreparedRealtimeInputKind> MediaPreparedRealtimeInput::kind() const noexcept
{
    if (m_genericBuffer) return MediaPreparedRealtimeInputKind::Generic;
    if (m_tsBuffer) return MediaPreparedRealtimeInputKind::MpegTs;
    return std::nullopt;
}

const FFmpegInputStreamSnapshot* MediaPreparedRealtimeInput::inputStreamSnapshot(int streamIndex) const noexcept
{
    if (m_genericBuffer) return m_genericBuffer->inputStreamSnapshot(streamIndex);
    if (!m_tsBuffer) return nullptr;
    const auto& snapshots = m_tsBuffer->streamSnapshots();
    const auto found = std::find_if(snapshots.begin(), snapshots.end(),
        [streamIndex](const FFmpegInputStreamSnapshot& snapshot) {
            return snapshot.index == streamIndex;
        });
    return found == snapshots.end() ? nullptr : &*found;
}

::media::Result<MediaBufferRef> MediaPreparedRealtimeInput::releaseBuffer()
{
    if (m_genericBuffer) return ::media::Result<MediaBufferRef>::success(MediaBufferRef(m_genericBuffer.release()));
    if (m_tsBuffer) {
        if (auto materialized = m_tsBuffer->materializeSession(); !materialized) {
            return ::media::Result<MediaBufferRef>::failure(materialized.error());
        }
        return ::media::Result<MediaBufferRef>::success(
            MediaBufferRef(m_tsBuffer.release()));
    }
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::notInitialized("prepared realtime input was already transferred"));
}

} // namespace media::ffmpeg::graph
