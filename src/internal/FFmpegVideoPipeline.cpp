#include "internal/FFmpegVideoPipeline.h"
#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

FFmpegVideoPipeline::~FFmpegVideoPipeline() {
    reset();
}

FFmpegVideoPipeline::FFmpegVideoPipeline(FFmpegVideoPipeline&& other) noexcept {
    *this = std::move(other);
}

FFmpegVideoPipeline& FFmpegVideoPipeline::operator=(FFmpegVideoPipeline&& other) noexcept {
    if (this == &other) return *this;

    reset();

    m_encoderCtx = other.m_encoderCtx;
    m_outputFmtCtx = other.m_outputFmtCtx;
    m_outputVideoStream = other.m_outputVideoStream;

    other.m_encoderCtx = nullptr;
    other.m_outputFmtCtx = nullptr;
    other.m_outputVideoStream = nullptr;

    return *this;
}

void FFmpegVideoPipeline::reset() {
    if (m_encoderCtx) {
        avcodec_free_context(&m_encoderCtx);
    }
    m_encoderCtx = nullptr;
    m_outputFmtCtx = nullptr;
    m_outputVideoStream = nullptr;
}

bool FFmpegVideoPipeline::initialize(const Config& config, std::string* error) {
    reset();

    if (!config.encoderCtx || !config.outputFmtCtx || !config.outputVideoStream) {
        if (error) *error = "FFmpegVideoPipeline initialize failed: invalid config";
        return false;
    }

    m_encoderCtx = config.encoderCtx;
    m_outputFmtCtx = config.outputFmtCtx;
    m_outputVideoStream = config.outputVideoStream;

    return true;
}

bool FFmpegVideoPipeline::sendFrame(AVFrame* frame, std::string* error) {
    if (!m_encoderCtx) {
        if (error) *error = "sendFrame failed: encoderCtx is null";
        return false;
    }

    int ret = avcodec_send_frame(m_encoderCtx, frame);
    if (ret < 0) {
        if (error) *error = "avcodec_send_frame failed: " + ffmpeg::errorString(ret);
        return false;
    }

    return true;
}

int FFmpegVideoPipeline::receiveAndWritePackets(std::string* error) {
    if (!m_encoderCtx || !m_outputFmtCtx || !m_outputVideoStream) return -1;

    int packetsWritten = 0;

    while (true) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            if (error) *error = "av_packet_alloc failed";
            return -1;
        }

        int ret = avcodec_receive_packet(m_encoderCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        }
        if (ret < 0) {
            if (error) *error = "avcodec_receive_packet failed: " + ffmpeg::errorString(ret);
            av_packet_free(&pkt);
            return -1;
        }

        pkt->stream_index = m_outputVideoStream->index;
        av_packet_rescale_ts(pkt, m_encoderCtx->time_base, m_outputVideoStream->time_base);

        ret = av_interleaved_write_frame(m_outputFmtCtx, pkt);
        av_packet_free(&pkt);
        if (ret < 0) {
            if (error) *error = "av_interleaved_write_frame failed: " + ffmpeg::errorString(ret);
            return -1;
        }

        ++packetsWritten;
    }

    return packetsWritten;
}

bool FFmpegVideoPipeline::isInitialized() const {
    return m_encoderCtx && m_outputFmtCtx && m_outputVideoStream;
}

} // namespace media::ffmpeg