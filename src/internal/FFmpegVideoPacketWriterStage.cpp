#include "internal/FFmpegVideoPacketWriterStage.h"

#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <sstream>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media::ffmpeg {

FFmpegVideoPacketWriterStage::~FFmpegVideoPacketWriterStage()
{
    reset();
}

FFmpegVideoPacketWriterStage::FFmpegVideoPacketWriterStage(
    FFmpegVideoPacketWriterStage&& other) noexcept
{
    *this = std::move(other);
}

FFmpegVideoPacketWriterStage& FFmpegVideoPacketWriterStage::operator=(
    FFmpegVideoPacketWriterStage&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_encoderCtx = other.m_encoderCtx;
    m_outputVideoStream = other.m_outputVideoStream;
    m_packetSink = other.m_packetSink;
    m_packetCount = other.m_packetCount;
    m_lastWrittenDts = other.m_lastWrittenDts;
    m_lastWrittenOutTimeMs = other.m_lastWrittenOutTimeMs;

    other.m_encoderCtx = nullptr;
    other.m_outputVideoStream = nullptr;
    other.m_packetSink = nullptr;
    other.m_packetCount = 0;
    other.m_lastWrittenDts = AV_NOPTS_VALUE;
    other.m_lastWrittenOutTimeMs = 0;

    return *this;
}

void FFmpegVideoPacketWriterStage::reset()
{
    m_encoderCtx = nullptr;
    m_outputVideoStream = nullptr;
    m_packetSink = nullptr;

    m_packetCount = 0;
    m_lastWrittenDts = AV_NOPTS_VALUE;
    m_lastWrittenOutTimeMs = 0;
}

Status FFmpegVideoPacketWriterStage::initialize(const Config& config)
{
    reset();

    if (!config.encoderCtx) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoPacketWriterStage initialize failed: encoderCtx is null"));
    }

    if (!config.outputVideoStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoPacketWriterStage initialize failed: outputVideoStream is null"));
    }

    if (!config.packetSink) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoPacketWriterStage initialize failed: packetSink is null"));
    }

    m_encoderCtx = config.encoderCtx;
    m_outputVideoStream = config.outputVideoStream;
    m_packetSink = config.packetSink;

    return Status::success();
}

Status FFmpegVideoPacketWriterStage::sendFrame(AVFrame* frame)
{
    if (!m_encoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegVideoPacketWriterStage sendFrame failed: encoderCtx is null"));
    }

    const int ret = avcodec_send_frame(m_encoderCtx, frame);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_frame encoder failed", ret));
    }

    return Status::success();
}

Result<int> FFmpegVideoPacketWriterStage::receiveAndWritePackets(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_encoderCtx || !m_outputVideoStream || !m_packetSink) {
        return Result<int>::failure(ErrorInfo::notInitialized(
            "FFmpegVideoPacketWriterStage receiveAndWritePackets failed: stage is not initialized"));
    }

    int packetsWritten = 0;

    while (true) {
        PacketPtr packet = makePacket();
        if (!packet) {
            return Result<int>::failure(makeAllocationError(
                "av_packet_alloc video packet failed"));
        }

        const int receiveRet = avcodec_receive_packet(m_encoderCtx, packet.get());

        if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
            break;
        }

        if (receiveRet < 0) {
            return Result<int>::failure(makeFFmpegError(
                "avcodec_receive_packet encoder failed", receiveRet));
        }

        packet->stream_index = m_outputVideoStream->index;

        if (packet->duration <= 0) {
            packet->duration = 1;
        }

        av_packet_rescale_ts(
            packet.get(),
            m_encoderCtx->time_base,
            m_outputVideoStream->time_base
        );

        if (packet->dts != AV_NOPTS_VALUE) {
            if (m_lastWrittenDts != AV_NOPTS_VALUE &&
                packet->dts <= m_lastWrittenDts) {
                std::ostringstream oss;
                oss << "encoded video packet dts is not strictly increasing: current="
                    << packet->dts
                    << ", last="
                    << m_lastWrittenDts;

                return Result<int>::failure(ErrorInfo::internalError(oss.str()));
            }

            m_lastWrittenDts = packet->dts;
        }

        if (packet->pts != AV_NOPTS_VALUE &&
            packet->dts != AV_NOPTS_VALUE &&
            packet->pts < packet->dts) {
            std::ostringstream oss;
            oss << "encoded video packet pts is smaller than dts: pts="
                << packet->pts
                << ", dts="
                << packet->dts;

            return Result<int>::failure(ErrorInfo::internalError(oss.str()));
        }

        if (packet->duration <= 0) {
            packet->duration = 1;
        }

        const int64_t progressTs = packet->pts != AV_NOPTS_VALUE
            ? packet->pts
            : packet->dts;

        if (progressTs != AV_NOPTS_VALUE) {
            const int64_t outTimeMs = av_rescale_q(
                progressTs,
                m_outputVideoStream->time_base,
                AVRational{ 1, 1000 }
            );

            m_lastWrittenOutTimeMs = std::max<int64_t>(
                m_lastWrittenOutTimeMs,
                outTimeMs
            );
        }

        const Status writeStatus = m_packetSink->writePacket(packet.get());
        if (!writeStatus) {
            return Result<int>::failure(writeStatus.error());
        }

        ++m_packetCount;
        ++packetsWritten;

        if (onPacketWritten) {
            onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
        }
    }

    return Result<int>::success(packetsWritten);
}

bool FFmpegVideoPacketWriterStage::isInitialized() const
{
    return m_encoderCtx && m_outputVideoStream && m_packetSink;
}

int64_t FFmpegVideoPacketWriterStage::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegVideoPacketWriterStage::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

} // namespace media::ffmpeg
