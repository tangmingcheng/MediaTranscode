#include "internal/output/sessions/file/FFmpegFileOutputSession.h"

#include "internal/FFmpegError.h"

#include <utility>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

FFmpegFileOutputSession::~FFmpegFileOutputSession()
{
    reset();
}

void FFmpegFileOutputSession::reset()
{
    m_outputGraphController.reset();
    m_fileOutputNode.reset();
    m_outputFmtCtx.reset();
    m_outputUrl.clear();
    m_headerWritten = false;
}

Status FFmpegFileOutputSession::initialize(Config config)
{
    reset();

    if (config.outputUrl.empty()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegFileOutputSession initialize failed: outputUrl is empty"));
    }

    AVFormatContext* rawOutputFmtCtx = nullptr;
    const int ret = avformat_alloc_output_context2(
        &rawOutputFmtCtx,
        nullptr,
        nullptr,
        config.outputUrl.c_str()
    );
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avformat_alloc_output_context2 failed",
            ret));
    }

    if (!rawOutputFmtCtx) {
        return Status::failure(ErrorInfo::internalError(
            "avformat_alloc_output_context2 failed: no output context allocated"));
    }

    m_outputUrl = std::move(config.outputUrl);
    m_outputFmtCtx.reset(rawOutputFmtCtx);

    FFmpegFileOutputNode::Config fileOutputConfig;
    fileOutputConfig.outputFmtCtx = m_outputFmtCtx.get();

    Status status = m_fileOutputNode.initialize(fileOutputConfig);
    if (!status) {
        reset();
        return status;
    }

    status = m_outputGraphController.attachExternalNode(&m_fileOutputNode);
    if (!status) {
        reset();
        return status;
    }

    return Status::success();
}

Status FFmpegFileOutputSession::openIo()
{
    if (!m_outputFmtCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegFileOutputSession openIo failed: output context is null"));
    }

    if (!m_outputFmtCtx->oformat) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegFileOutputSession openIo failed: output format is null"));
    }

    if (m_outputFmtCtx->oformat->flags & AVFMT_NOFILE) {
        return Status::success();
    }

    if (m_outputFmtCtx->pb) {
        return Status::success();
    }

    const int ret = avio_open(&m_outputFmtCtx->pb, m_outputUrl.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("avio_open output failed", ret));
    }

    return Status::success();
}

Status FFmpegFileOutputSession::writeHeader()
{
    if (!m_outputFmtCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegFileOutputSession writeHeader failed: output context is null"));
    }

    if (m_headerWritten) {
        return Status::success();
    }

    const int ret = avformat_write_header(m_outputFmtCtx.get(), nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("avformat_write_header failed", ret));
    }

    m_headerWritten = true;
    return Status::success();
}

Status FFmpegFileOutputSession::writeTrailer()
{
    if (!m_outputFmtCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegFileOutputSession writeTrailer failed: output context is null"));
    }

    if (!m_headerWritten) {
        return Status::success();
    }

    const int ret = av_write_trailer(m_outputFmtCtx.get());
    if (ret < 0) {
        return Status::failure(makeFFmpegError("av_write_trailer failed", ret));
    }

    m_headerWritten = false;
    return Status::success();
}

bool FFmpegFileOutputSession::isInitialized() const
{
    return m_outputFmtCtx && !m_outputGraphController.empty();
}

bool FFmpegFileOutputSession::headerWritten() const
{
    return m_headerWritten;
}

AVFormatContext* FFmpegFileOutputSession::context() const
{
    return m_outputFmtCtx.get();
}

VideoOutputStreamProvider* FFmpegFileOutputSession::videoStreamProvider()
{
    return &m_fileOutputNode;
}

AudioOutputStreamProvider* FFmpegFileOutputSession::audioStreamProvider()
{
    return &m_fileOutputNode;
}

PacketOutputNode* FFmpegFileOutputSession::outputNode()
{
    return m_outputGraphController.rootNode();
}

} // namespace media::ffmpeg
