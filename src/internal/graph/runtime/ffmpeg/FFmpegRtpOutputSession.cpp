#include "internal/graph/runtime/ffmpeg/FFmpegRtpOutputSession.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

FFmpegRtpOutputSession::~FFmpegRtpOutputSession()
{
    close();
}

::media::Status FFmpegRtpOutputSession::open(FFmpegRtpOutputSessionOptions options)
{
    close();
    m_interrupted = false;
    m_options = std::move(options);
    if (m_options.url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegRtpOutputSession requires output URL"));
    }

    AVFormatContext* raw = nullptr;
    const int allocRet = avformat_alloc_output_context2(&raw, nullptr, "rtp", m_options.url.c_str());
    if (allocRet < 0 || !raw) {
        return FFmpegGraphError::statusFromCode(allocRet < 0 ? allocRet : AVERROR_UNKNOWN,
                                                "avformat_alloc_output_context2(rtp)");
    }

    raw->interrupt_callback.callback = &FFmpegRtpOutputSession::interruptCallback;
    raw->interrupt_callback.opaque = this;
    m_context.reset(raw);

    if (m_context->oformat && !(m_context->oformat->flags & AVFMT_NOFILE)) {
        const int openRet = avio_open(&m_context->pb, m_options.url.c_str(), AVIO_FLAG_WRITE);
        if (openRet < 0) {
            close();
            return FFmpegGraphError::statusFromCode(openRet, "avio_open(rtp)");
        }
    }

    return ::media::Status::success();
}

void FFmpegRtpOutputSession::close() noexcept
{
    m_context.reset();
}

void FFmpegRtpOutputSession::interrupt() noexcept
{
    m_interrupted = true;
}

AVFormatContext* FFmpegRtpOutputSession::context() noexcept
{
    return m_context.get();
}

::media::ffmpeg::OutputFormatContextPtr FFmpegRtpOutputSession::takeContext() noexcept
{
    return std::move(m_context);
}

int FFmpegRtpOutputSession::interruptCallback(void* opaque) noexcept
{
    auto* self = static_cast<FFmpegRtpOutputSession*>(opaque);
    return self && self->m_interrupted ? 1 : 0;
}

} // namespace media::ffmpeg::graph
