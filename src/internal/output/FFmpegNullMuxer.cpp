#include "internal/output/FFmpegNullMuxer.h"

#include "internal/FFmpegError.h"

namespace media::ffmpeg {

FFmpegNullMuxer::~FFmpegNullMuxer()
{
    reset();
}

Status FFmpegNullMuxer::open()
{
    reset();

    AVFormatContext* rawContext = nullptr;
    const int ret = avformat_alloc_output_context2(&rawContext, nullptr, "null", nullptr);
    if (ret < 0 || !rawContext) {
        return Status::failure(makeFFmpegError(
            "avformat_alloc_output_context2 null muxer failed",
            ret));
    }

    m_context.reset(rawContext);
    return Status::success();
}

Status FFmpegNullMuxer::writeHeader()
{
    if (!m_context) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegNullMuxer writeHeader failed: muxer is not open"));
    }

    if (m_headerWritten) {
        return Status::success();
    }

    const int ret = avformat_write_header(m_context.get(), nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avformat_write_header null muxer failed",
            ret));
    }

    m_headerWritten = true;
    return Status::success();
}

Status FFmpegNullMuxer::writeTrailer()
{
    if (!m_context || !m_headerWritten) {
        return Status::success();
    }

    const int ret = av_write_trailer(m_context.get());
    m_headerWritten = false;

    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "av_write_trailer null muxer failed",
            ret));
    }

    return Status::success();
}

void FFmpegNullMuxer::reset()
{
    if (m_context && m_headerWritten) {
        av_write_trailer(m_context.get());
        m_headerWritten = false;
    }
    m_context.reset();
}

AVFormatContext* FFmpegNullMuxer::context() const
{
    return m_context.get();
}

bool FFmpegNullMuxer::isOpen() const
{
    return m_context != nullptr;
}

bool FFmpegNullMuxer::headerWritten() const
{
    return m_headerWritten;
}

} // namespace media::ffmpeg
