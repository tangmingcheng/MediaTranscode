#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRawRtpPreparedInputBuffer::MediaRawRtpPreparedInputBuffer(
    MediaPreparedRawRtpInput prepared)
    : m_prepared(std::move(prepared))
{
    setStreamKind(MediaStreamKind::Video);
    setPayloadKind(MediaPayloadKind::FormatContext);
    setDiagnosticName("MediaRawRtpPreparedInputBuffer");
}

::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>
MediaRawRtpPreparedInputBuffer::create(MediaPreparedRawRtpInput prepared)
{
    if (!prepared.transport.isOpen() || prepared.datagrams.empty() ||
        prepared.signaling.packetCount == 0 ||
        prepared.signaling.datagramBytes == 0) {
        return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared input requires open transport, buffered data, and signaling evidence"));
    }
    return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::success(
        std::unique_ptr<MediaRawRtpPreparedInputBuffer>(
            new MediaRawRtpPreparedInputBuffer(std::move(prepared))));
}

MediaBufferType MediaRawRtpPreparedInputBuffer::type() const noexcept
{
    return MediaBufferType::RawRtpPreparedInput;
}

::media::Result<MediaPreparedRawRtpInput>
MediaRawRtpPreparedInputBuffer::takePreparedInput()
{
    if (!m_prepared) {
        return ::media::Result<MediaPreparedRawRtpInput>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared input was already transferred"));
    }
    MediaPreparedRawRtpInput prepared = std::move(*m_prepared);
    m_prepared.reset();
    return ::media::Result<MediaPreparedRawRtpInput>::success(
        std::move(prepared));
}

} // namespace media::ffmpeg::graph
