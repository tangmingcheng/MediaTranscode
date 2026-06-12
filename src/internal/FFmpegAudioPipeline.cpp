#include "internal/FFmpegAudioPipeline.h"

#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/avutil.h>
}

namespace media::ffmpeg {

FFmpegAudioPipeline::~FFmpegAudioPipeline()
{
    reset();
}

FFmpegAudioPipeline::FFmpegAudioPipeline(FFmpegAudioPipeline&& other) noexcept
{
    *this = std::move(other);
}

FFmpegAudioPipeline& FFmpegAudioPipeline::operator=(FFmpegAudioPipeline&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_encoderCtx = other.m_encoderCtx;
    m_outputFmtCtx = other.m_outputFmtCtx;
    m_outputAudioStream = other.m_outputAudioStream;
    m_packetCount = other.m_packetCount;
    m_lastWrittenDts = other.m_lastWrittenDts;
    m_lastWrittenOutTimeMs = other.m_lastWrittenOutTimeMs;

    other.m_encoderCtx = nullptr;
    other.m_outputFmtCtx = nullptr;
    other.m_outputAudioStream = nullptr;
    other.m_packetCount = 0;
    other.m_lastWrittenDts = AV_NOPTS_VALUE;
    other.m_lastWrittenOutTimeMs = 0;

    return *this;
}

void FFmpegAudioPipeline::reset()
{
    /*
     * FFmpegAudioPipeline does not own these contexts.
     * They are allocated and released by FFmpegTranscoder.
     */
    m_encoderCtx = nullptr;
    m_outputFmtCtx = nullptr;
    m_outputAudioStream = nullptr;

    m_packetCount = 0;
    m_lastWrittenDts = AV_NOPTS_VALUE;
    m_lastWrittenOutTimeMs = 0;
}

bool FFmpegAudioPipeline::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.encoderCtx) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: encoderCtx is null";
        }
        return false;
    }

    if (!config.outputFmtCtx) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: outputFmtCtx is null";
        }
        return false;
    }

    if (!config.outputAudioStream) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: outputAudioStream is null";
        }
        return false;
    }

    m_encoderCtx = config.encoderCtx;
    m_outputFmtCtx = config.outputFmtCtx;
    m_outputAudioStream = config.outputAudioStream;

    return true;
}

bool FFmpegAudioPipeline::sendFrame(AVFrame* frame, std::string* error)
{
    if (!m_encoderCtx) {
        if (error) {
            *error = "FFmpegAudioPipeline sendFrame failed: encoderCtx is null";
        }
        return false;
    }

    const int ret = avcodec_send_frame(m_encoderCtx, frame);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_frame audio encoder failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

int FFmpegAudioPipeline::receiveAndWritePackets(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_encoderCtx || !m_outputFmtCtx || !m_outputAudioStream) {
        if (error) {
            *error = "FFmpegAudioPipeline receiveAndWritePackets failed: pipeline is not initialized";
        }
        return -1;
    }

    int packetsWritten = 0;

    while (true) {
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            if (error) {
                *error = "av_packet_alloc audio packet failed";
            }
            return -1;
        }

        const int receiveRet = avcodec_receive_packet(m_encoderCtx, packet);

        if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
            av_packet_free(&packet);
            break;
        }

        if (receiveRet < 0) {
            if (error) {
                *error = "avcodec_receive_packet audio encoder failed: " + errorString(receiveRet);
            }
            av_packet_free(&packet);
            return -1;
        }

        packet->stream_index = m_outputAudioStream->index;

        av_packet_rescale_ts(
            packet,
            m_encoderCtx->time_base,
            m_outputAudioStream->time_base
        );

        if (packet->dts != AV_NOPTS_VALUE) {
            if (m_lastWrittenDts != AV_NOPTS_VALUE &&
                packet->dts <= m_lastWrittenDts) {
                std::ostringstream oss;
                oss << "encoded audio packet dts is not strictly increasing: current="
                    << packet->dts
                    << ", last="
                    << m_lastWrittenDts;

                if (error) {
                    *error = oss.str();
                }
                av_packet_free(&packet);
                return -1;
            }

            m_lastWrittenDts = packet->dts;
        }

        if (packet->pts != AV_NOPTS_VALUE &&
            packet->dts != AV_NOPTS_VALUE &&
            packet->pts < packet->dts) {
            std::ostringstream oss;
            oss << "encoded audio packet pts is smaller than dts: pts="
                << packet->pts
                << ", dts="
                << packet->dts;

            if (error) {
                *error = oss.str();
            }
            av_packet_free(&packet);
            return -1;
        }

        const int64_t progressTs = packet->pts != AV_NOPTS_VALUE
            ? packet->pts
            : packet->dts;

        if (progressTs != AV_NOPTS_VALUE) {
            const int64_t outTimeMs = av_rescale_q(
                progressTs,
                m_outputAudioStream->time_base,
                AVRational{ 1, 1000 }
            );

            m_lastWrittenOutTimeMs = std::max<int64_t>(
                m_lastWrittenOutTimeMs,
                outTimeMs
            );
        }

        const int writeRet = av_interleaved_write_frame(m_outputFmtCtx, packet);

        av_packet_free(&packet);

        if (writeRet < 0) {
            if (error) {
                *error = "av_interleaved_write_frame encoded audio failed: " + errorString(writeRet);
            }
            return -1;
        }

        ++m_packetCount;
        ++packetsWritten;

        if (onPacketWritten) {
            onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
        }
    }

    return packetsWritten;
}

bool FFmpegAudioPipeline::isInitialized() const
{
    return m_encoderCtx && m_outputFmtCtx && m_outputAudioStream;
}

int64_t FFmpegAudioPipeline::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegAudioPipeline::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

} // namespace media::ffmpeg
