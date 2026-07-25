#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h"

#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSinkBackend.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avio.h>
}

#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validWriteFlags(int flags) noexcept
{
    constexpr int supportedFlags = AVIO_FLAG_WRITE | AVIO_FLAG_NONBLOCK | AVIO_FLAG_DIRECT;
    return (flags & AVIO_FLAG_WRITE) != 0 &&
           (flags & AVIO_FLAG_READ) == 0 &&
           (flags & ~supportedFlags) == 0;
}

} // namespace

FFmpegAvioOutputByteSink::FFmpegAvioOutputByteSink(
    std::unique_ptr<FFmpegAvioOutputByteSinkBackend> backend) noexcept
    : m_backend(std::move(backend))
{
}

::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>
FFmpegAvioOutputByteSink::open(std::string url, int writeFlags)
{
    if (url.empty()) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::invalidArgument("output byte sink URL must not be empty"));
    }
    if (!validWriteFlags(writeFlags)) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "output byte sink requires supported write-only AVIO flags"));
    }

    auto backend = FFmpegAvioOutputByteSinkBackend::open(
        std::move(url), writeFlags);
    if (!backend) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            backend.error());
    }
    return create(std::move(backend).value());
}

::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>
FFmpegAvioOutputByteSink::create(
    std::unique_ptr<FFmpegAvioOutputByteSinkBackend> backend)
{
    if (!backend) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "output byte sink requires an owned AVIO backend"));
    }
    auto sink = std::unique_ptr<FFmpegAvioOutputByteSink>(
        new (std::nothrow) FFmpegAvioOutputByteSink(std::move(backend)));
    if (!sink) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::allocationFailed("FFmpegAvioOutputByteSink"));
    }
    return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::success(
        std::move(sink));
}

FFmpegAvioOutputByteSink::~FFmpegAvioOutputByteSink() noexcept
{
    if (!m_closed && m_backend) {
        static_cast<void>(m_backend->close());
        m_closed = true;
    }
}

::media::Result<std::size_t> FFmpegAvioOutputByteSink::write(
    std::span<const std::uint8_t> bytes)
{
    if (m_firstFailure) {
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }
    if (m_closed || !m_backend) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized("output byte sink is closed"));
    }
    if (bytes.empty()) {
        preserveFailure(
            ::media::ErrorInfo::invalidArgument("output byte sink write must not be empty"));
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }
    if (bytes.size() > m_backend->maximumWriteBytes()) {
        preserveFailure(::media::ErrorInfo::invalidArgument(
            "output byte sink write exceeds the backend capacity"));
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }

    m_backend->write(bytes);
    m_backend->flush();
    if (m_backend->error() < 0) {
        preserveFailure(FFmpegGraphError::fromCode(
            m_backend->error(), "avio_write/avio_flush(output byte sink)"));
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }
    return ::media::Result<std::size_t>::success(bytes.size());
}

::media::Status FFmpegAvioOutputByteSink::flush()
{
    if (m_firstFailure) return currentStatus();
    if (m_closed || !m_backend) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("output byte sink is closed"));
    }

    m_backend->flush();
    if (m_backend->error() < 0) {
        preserveFailure(FFmpegGraphError::fromCode(
            m_backend->error(), "avio_flush(output byte sink)"));
    }
    return currentStatus();
}

::media::Status FFmpegAvioOutputByteSink::close()
{
    if (m_closed) return currentStatus();

    if (m_backend) {
        const int closeResult = m_backend->close();
        if (closeResult < 0) {
            preserveFailure(FFmpegGraphError::fromCode(
                closeResult, "avio_closep(output byte sink)"));
        }
    }
    m_closed = true;
    return currentStatus();
}

::media::Status FFmpegAvioOutputByteSink::currentStatus() const
{
    return m_firstFailure
        ? ::media::Status::failure(*m_firstFailure)
        : ::media::Status::success();
}

void FFmpegAvioOutputByteSink::preserveFailure(::media::ErrorInfo error)
{
    if (!m_firstFailure) m_firstFailure = std::move(error);
}

} // namespace media::ffmpeg::graph
