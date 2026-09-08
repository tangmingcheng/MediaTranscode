#include "internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <cerrno>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

FFmpegDatagramWriteAvioConfig::FFmpegDatagramWriteAvioConfig(
    int maximumDatagramBytes) noexcept
    : m_maximumDatagramBytes(maximumDatagramBytes)
{
}

::media::Result<FFmpegDatagramWriteAvioConfig>
FFmpegDatagramWriteAvioConfig::create(int maximumDatagramBytes) noexcept
{
    if (maximumDatagramBytes <= 12) {
        return ::media::Result<FFmpegDatagramWriteAvioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram AVIO maximum datagram size must exceed the RTP fixed header"));
    }
    return ::media::Result<FFmpegDatagramWriteAvioConfig>::success(
        FFmpegDatagramWriteAvioConfig(maximumDatagramBytes));
}

FFmpegDatagramWriteAvio::FFmpegDatagramWriteAvio(
    FFmpegDatagramWriteAvioConfig config,
    FFmpegDatagramSink sink)
    : m_config(config),
      m_sink(std::move(sink))
{
}

::media::Result<std::unique_ptr<FFmpegDatagramWriteAvio>>
FFmpegDatagramWriteAvio::create(FFmpegDatagramWriteAvioConfig config,
                                FFmpegDatagramSink sink)
{
    if (!sink) {
        return ::media::Result<std::unique_ptr<FFmpegDatagramWriteAvio>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram AVIO requires a synchronous datagram sink"));
    }
    auto result = std::unique_ptr<FFmpegDatagramWriteAvio>(
        new (std::nothrow) FFmpegDatagramWriteAvio(config, std::move(sink)));
    if (!result) {
        return ::media::Result<std::unique_ptr<FFmpegDatagramWriteAvio>>::failure(
            ::media::ErrorInfo::allocationFailed("FFmpegDatagramWriteAvio"));
    }
    return ::media::Result<std::unique_ptr<FFmpegDatagramWriteAvio>>::success(
        std::move(result));
}

FFmpegDatagramWriteAvio::~FFmpegDatagramWriteAvio()
{
    release();
}

::media::Status FFmpegDatagramWriteAvio::open() noexcept
{
    if (m_context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("datagram AVIO is already open"));
    }
    if (m_sinkFailure) {
        return ::media::Status::failure(*m_sinkFailure);
    }

    auto* buffer = static_cast<unsigned char*>(
        av_malloc(static_cast<std::size_t>(m_config.maximumDatagramBytes())));
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("datagram AVIO buffer"));
    }
    m_context = avio_alloc_context(buffer,
                                   m_config.maximumDatagramBytes(),
                                   1,
                                   this,
                                   nullptr,
                                   &FFmpegDatagramWriteAvio::writePacket,
                                   nullptr);
    if (!m_context) {
        av_free(buffer);
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("avio_alloc_context(datagram write)"));
    }
    m_context->seekable = 0;
    m_context->direct = 0;
    m_context->max_packet_size = m_config.maximumDatagramBytes();
    return ::media::Status::success();
}

::media::Status FFmpegDatagramWriteAvio::close() noexcept
{
    if (!m_context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("datagram AVIO is not open"));
    }
    avio_flush(m_context);
    const auto failure = m_sinkFailure;
    release();
    return failure
        ? ::media::Status::failure(*failure)
        : ::media::Status::success();
}

::media::Status FFmpegDatagramWriteAvio::reset() noexcept
{
    if (m_context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram AVIO must be closed before reset"));
    }
    m_sinkFailure.reset();
    return ::media::Status::success();
}

int FFmpegDatagramWriteAvio::writePacket(void* opaque,
                                         FFmpegAvioWritePacketByte* bytes,
                                         int size) noexcept
{
    auto* self = static_cast<FFmpegDatagramWriteAvio*>(opaque);
    if (!self || !bytes || size <= 0) {
        return AVERROR(EINVAL);
    }
    return self->deliver(
        std::span<const std::uint8_t>(bytes, static_cast<std::size_t>(size)));
}

int FFmpegDatagramWriteAvio::deliver(
    std::span<const std::uint8_t> bytes) noexcept
{
    if (m_sinkFailure) {
        return AVERROR_EXTERNAL;
    }
    if (bytes.size() > static_cast<std::size_t>(m_config.maximumDatagramBytes())) {
        m_sinkFailure = ::media::ErrorInfo::invalidArgument(
            "FFmpeg emitted a datagram larger than the configured maximum");
        return AVERROR(EINVAL);
    }
    try {
        auto status = m_sink(bytes);
        if (!status) {
            m_sinkFailure = status.error();
            return AVERROR_EXTERNAL;
        }
    } catch (...) {
        m_sinkFailure = ::media::ErrorInfo::internalError(
            "datagram sink threw across the FFmpeg callback boundary");
        return AVERROR_EXTERNAL;
    }
    return static_cast<int>(bytes.size());
}

void FFmpegDatagramWriteAvio::release() noexcept
{
    if (m_context) {
        av_freep(&m_context->buffer);
        avio_context_free(&m_context);
    }
}

} // namespace media::ffmpeg::graph
