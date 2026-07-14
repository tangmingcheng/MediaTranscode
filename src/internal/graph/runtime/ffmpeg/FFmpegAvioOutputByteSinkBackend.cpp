#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSinkBackend.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avio.h>
}

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class FFmpegAvioOutputByteSinkBackendImpl final
    : public FFmpegAvioOutputByteSinkBackend {
public:
    explicit FFmpegAvioOutputByteSinkBackendImpl(AVIOContext* context) noexcept
        : m_context(context)
    {
    }

    ~FFmpegAvioOutputByteSinkBackendImpl() override
    {
        static_cast<void>(close());
    }

    void write(std::span<const std::uint8_t> bytes) override
    {
        avio_write(m_context, bytes.data(), static_cast<int>(bytes.size()));
        captureError();
    }

    void flush() override
    {
        avio_flush(m_context);
        captureError();
    }

    int error() const noexcept override
    {
        return m_error;
    }

    std::size_t maximumWriteBytes() const noexcept override
    {
        return static_cast<std::size_t>(std::numeric_limits<int>::max());
    }

    int close() noexcept override
    {
        if (!m_context) return m_closeResult;
        m_closeResult = avio_closep(&m_context);
        return m_closeResult;
    }

private:
    void captureError() noexcept
    {
        if (m_error >= 0 && m_context->error < 0) m_error = m_context->error;
    }

    AVIOContext* m_context;
    int m_error = 0;
    int m_closeResult = 0;
};

} // namespace

::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>>
FFmpegAvioOutputByteSinkBackend::open(std::string url, int writeFlags)
{
    AVIOContext* context = nullptr;
    const int openResult = avio_open2(
        &context, url.c_str(), writeFlags, nullptr, nullptr);
    if (openResult < 0) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>>::failure(
            FFmpegGraphError::fromCode(openResult, "avio_open2(output byte sink)"));
    }
    if (!context) {
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>>::failure(
            ::media::ErrorInfo::internalError(
                "avio_open2 succeeded without an output context"));
    }

    auto backend = std::unique_ptr<FFmpegAvioOutputByteSinkBackend>(
        new (std::nothrow) FFmpegAvioOutputByteSinkBackendImpl(context));
    if (!backend) {
        avio_closep(&context);
        return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "FFmpegAvioOutputByteSinkBackend"));
    }
    return ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>>::success(
        std::move(backend));
}

} // namespace media::ffmpeg::graph
