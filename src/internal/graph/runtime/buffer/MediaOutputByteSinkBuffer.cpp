#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaOutputByteSinkBuffer::MediaOutputByteSinkBuffer(
    std::unique_ptr<MediaOutputByteSink> sink)
    : m_sink(std::move(sink))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::OutputByteSink);
    setDiagnosticName("output.byte_sink");
}

::media::Result<std::unique_ptr<MediaOutputByteSinkBuffer>>
MediaOutputByteSinkBuffer::create(std::unique_ptr<MediaOutputByteSink> sink)
{
    if (!sink) {
        return ::media::Result<std::unique_ptr<MediaOutputByteSinkBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "output byte sink buffer requires an owned sink"));
    }
    auto buffer = std::unique_ptr<MediaOutputByteSinkBuffer>(
        new (std::nothrow) MediaOutputByteSinkBuffer(std::move(sink)));
    if (!buffer) {
        return ::media::Result<std::unique_ptr<MediaOutputByteSinkBuffer>>::failure(
            ::media::ErrorInfo::allocationFailed("MediaOutputByteSinkBuffer"));
    }
    return ::media::Result<std::unique_ptr<MediaOutputByteSinkBuffer>>::success(
        std::move(buffer));
}

MediaBufferType MediaOutputByteSinkBuffer::type() const noexcept
{
    return MediaBufferType::OutputByteSink;
}

::media::Result<std::unique_ptr<MediaOutputByteSink>>
MediaOutputByteSinkBuffer::takeSink()
{
    if (!m_sink) {
        return ::media::Result<std::unique_ptr<MediaOutputByteSink>>::failure(
            ::media::ErrorInfo::notInitialized(
                "output byte sink was already transferred"));
    }
    return ::media::Result<std::unique_ptr<MediaOutputByteSink>>::success(
        std::move(m_sink));
}

} // namespace media::ffmpeg::graph
