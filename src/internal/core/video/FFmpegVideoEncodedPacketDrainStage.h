#pragma once

#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"
#include "internal/output/PacketOutputNode.h"
#include "media_transcode/Result.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media::ffmpeg {

class FFmpegVideoEncodedPacketDrainStage {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        AVCodecContext* encoderCtx = nullptr;
        AVStream* outputVideoStream = nullptr;
        PacketOutputNode* outputNode = nullptr;
    };

    FFmpegVideoEncodedPacketDrainStage() = default;
    ~FFmpegVideoEncodedPacketDrainStage() = default;

    FFmpegVideoEncodedPacketDrainStage(const FFmpegVideoEncodedPacketDrainStage&) = delete;
    FFmpegVideoEncodedPacketDrainStage& operator=(const FFmpegVideoEncodedPacketDrainStage&) = delete;

    void reset()
    {
        m_encoderCtx = nullptr;
        m_outputVideoStream = nullptr;
        m_outputNode = nullptr;

        m_packetCount = 0;
        m_lastWrittenDts = AV_NOPTS_VALUE;
        m_lastWrittenOutTimeMs = 0;
    }

    Status initialize(const Config& config)
    {
        reset();

        if (!config.encoderCtx) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegVideoEncodedPacketDrainStage initialize failed: encoderCtx is null"));
        }

        if (!config.outputVideoStream) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegVideoEncodedPacketDrainStage initialize failed: outputVideoStream is null"));
        }

        if (!config.outputNode) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegVideoEncodedPacketDrainStage initialize failed: outputNode is null"));
        }

        m_encoderCtx = config.encoderCtx;
        m_outputVideoStream = config.outputVideoStream;
        m_outputNode = config.outputNode;

        return Status::success();
    }

    Status sendFrame(AVFrame* frame)
    {
        if (!m_encoderCtx) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegVideoEncodedPacketDrainStage sendFrame failed: encoderCtx is null"));
        }

        const int ret = avcodec_send_frame(m_encoderCtx, frame);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "avcodec_send_frame encoder failed", ret));
        }

        return Status::success();
    }

    Result<int> receiveAndPushPackets(const PacketWrittenCallback& onPacketWritten = {})
    {
        if (!m_encoderCtx || !m_outputVideoStream || !m_outputNode) {
            return Result<int>::failure(ErrorInfo::notInitialized(
                "FFmpegVideoEncodedPacketDrainStage receiveAndPushPackets failed: stage is not initialized"));
        }

        int packetsPushed = 0;

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

            const Status pushStatus = m_outputNode->pushPacket(packet.get());
            if (!pushStatus) {
                return Result<int>::failure(pushStatus.error());
            }

            ++m_packetCount;
            ++packetsPushed;

            if (onPacketWritten) {
                onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
            }
        }

        return Result<int>::success(packetsPushed);
    }

    bool isInitialized() const
    {
        return m_encoderCtx && m_outputVideoStream && m_outputNode;
    }

    int64_t packetCount() const
    {
        return m_packetCount;
    }

    int64_t lastWrittenOutTimeMs() const
    {
        return m_lastWrittenOutTimeMs;
    }

private:
    AVCodecContext* m_encoderCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;
    PacketOutputNode* m_outputNode = nullptr;

    int64_t m_packetCount = 0;
    int64_t m_lastWrittenDts = AV_NOPTS_VALUE;
    int64_t m_lastWrittenOutTimeMs = 0;
};

} // namespace media::ffmpeg
