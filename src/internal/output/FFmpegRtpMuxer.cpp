#include "internal/output/FFmpegRtpMuxer.h"

#include "internal/FFmpegError.h"
#include "internal/output/FFmpegSdpWriter.h"

namespace media::ffmpeg {

namespace {

bool validPort(int port)
{
    return port > 0 && port <= 65535;
}

bool validOptionalPort(int port)
{
    return port == 0 || validPort(port);
}

Status validateConfig(const FFmpegRtpOutputConfig& config)
{
    if (config.host.empty()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegRtpMuxer open failed: host is empty"));
    }

    if (!validPort(config.rtpPort)) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegRtpMuxer open failed: RTP port must be in range 1..65535"));
    }

    if (!validOptionalPort(config.rtcpPort) ||
        !validOptionalPort(config.localRtpPort) ||
        !validOptionalPort(config.localRtcpPort)) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegRtpMuxer open failed: optional RTP/RTCP ports must be 0 or in range 1..65535"));
    }

    if (config.packetSize <= 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegRtpMuxer open failed: packetSize must be positive"));
    }

    return Status::success();
}

} // namespace

FFmpegRtpMuxer::~FFmpegRtpMuxer()
{
    reset();
}

Status FFmpegRtpMuxer::open(const FFmpegRtpOutputConfig& config)
{
    reset();

    Status validation = validateConfig(config);
    if (!validation) {
        return validation;
    }

    m_config = config;
    m_url = FFmpegRtpUrlBuilder::build(config);

    AVFormatContext* rawContext = nullptr;
    const int ret = avformat_alloc_output_context2(
        &rawContext,
        nullptr,
        "rtp",
        m_url.c_str()
    );

    if (ret < 0 || !rawContext) {
        return Status::failure(makeFFmpegError(
            "avformat_alloc_output_context2 RTP muxer failed",
            ret));
    }

    m_context.reset(rawContext);
    return Status::success();
}

Status FFmpegRtpMuxer::writeHeader()
{
    if (!m_context) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegRtpMuxer writeHeader failed: muxer is not open"));
    }

    if (m_headerWritten) {
        return Status::success();
    }

    if (!(m_context->oformat->flags & AVFMT_NOFILE)) {
        const int openRet = avio_open(&m_context->pb, m_url.c_str(), AVIO_FLAG_WRITE);
        if (openRet < 0) {
            return Status::failure(makeFFmpegError(
                "avio_open RTP output failed",
                openRet));
        }
    }

    const int ret = avformat_write_header(m_context.get(), nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avformat_write_header RTP muxer failed",
            ret));
    }

    m_headerWritten = true;
    return Status::success();
}

Status FFmpegRtpMuxer::writeSdp()
{
    if (!m_context) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegRtpMuxer writeSdp failed: muxer is not open"));
    }

    return FFmpegSdpWriter::save(m_context.get(), m_config.sdpOutputPath);
}

Status FFmpegRtpMuxer::writeTrailer()
{
    if (!m_context || !m_headerWritten) {
        return Status::success();
    }

    const int ret = av_write_trailer(m_context.get());
    m_headerWritten = false;

    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "av_write_trailer RTP muxer failed",
            ret));
    }

    return Status::success();
}

void FFmpegRtpMuxer::reset()
{
    if (m_context && m_headerWritten) {
        av_write_trailer(m_context.get());
        m_headerWritten = false;
    }

    m_context.reset();
    m_config = FFmpegRtpOutputConfig{};
    m_url.clear();
}

AVFormatContext* FFmpegRtpMuxer::context() const
{
    return m_context.get();
}

const std::string& FFmpegRtpMuxer::url() const
{
    return m_url;
}

bool FFmpegRtpMuxer::isOpen() const
{
    return m_context != nullptr;
}

bool FFmpegRtpMuxer::headerWritten() const
{
    return m_headerWritten;
}

} // namespace media::ffmpeg
