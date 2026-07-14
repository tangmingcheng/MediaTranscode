#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avio.h>
}

#include <limits>
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

FFmpegAvioOutputByteSink::FFmpegAvioOutputByteSink(AVIOContext* context) noexcept
    : m_context(context)
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

    AVIOContext* context = nullptr;
    const int openResult = avio_open2(
        &context, url.c_str(), writeFlags, nullptr, nullptr);
    if (openResult < 0) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            FFmpegGraphError::fromCode(openResult, "avio_open2(output byte sink)"));
    }
    if (!context) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::internalError(
                "avio_open2 succeeded without an output context"));
    }

    auto sink = std::unique_ptr<FFmpegAvioOutputByteSink>(
        new (std::nothrow) FFmpegAvioOutputByteSink(context));
    if (!sink) {
        avio_closep(&context);
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::failure(
            ::media::ErrorInfo::allocationFailed("FFmpegAvioOutputByteSink"));
    }
    return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>>::success(
        std::move(sink));
}

FFmpegAvioOutputByteSink::~FFmpegAvioOutputByteSink() noexcept
{
    if (!m_closed && m_context) {
        avio_closep(&m_context);
        m_closed = true;
    }
}

::media::Result<std::size_t> FFmpegAvioOutputByteSink::write(
    std::span<const std::uint8_t> bytes)
{
    if (m_closed || !m_context) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized("output byte sink is closed"));
    }
    if (m_firstFailure) {
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }
    if (bytes.empty()) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("output byte sink write must not be empty"));
    }
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "output byte sink write exceeds the FFmpeg AVIO size limit"));
    }

    avio_write(m_context, bytes.data(), static_cast<int>(bytes.size()));
    avio_flush(m_context);
    if (m_context->error < 0) {
        preserveFailure(FFmpegGraphError::fromCode(
            m_context->error, "avio_write(output byte sink)"));
        return ::media::Result<std::size_t>::failure(*m_firstFailure);
    }
    return ::media::Result<std::size_t>::success(bytes.size());
}

::media::Status FFmpegAvioOutputByteSink::flush()
{
    if (m_closed || !m_context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("output byte sink is closed"));
    }
    if (m_firstFailure) return currentStatus();

    avio_flush(m_context);
    if (m_context->error < 0) {
        preserveFailure(FFmpegGraphError::fromCode(
            m_context->error, "avio_flush(output byte sink)"));
    }
    return currentStatus();
}

::media::Status FFmpegAvioOutputByteSink::close()
{
    if (m_closed) return currentStatus();

    if (m_context) {
        const int closeResult = avio_closep(&m_context);
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
