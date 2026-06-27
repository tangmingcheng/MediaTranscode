#pragma once

#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"
#include "internal/graph/nodes/output/packet/PacketOutputNode.h"
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
                    "FFmpegVideoEncodedPacketDrainStage failed to allocate packet"));
            }

            const int ret = avcodec_receive_packet(m_encoderCtx, packet.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return Result<int>::success(packetsPushed);
            }

            if (ret < 0) {
                return Result<int>::failure(makeFFmpegError(
                    "avcodec_receive_packet encoder failed", ret));
            }

            av_packet_rescale_ts(packet.get(), m_encoderCtx->time_base, m_outputVideoStream->time_base);
            packet->stream_index = m_outputVideoStream->index;

            const Status timestampStatus = validateTimestamp(packet.get());
            if (!timestampStatus) {
                return Result<int>::failure(timestampStatus.error());
            }

            updateProgressFromPacket(packet.get());

            const Status writeStatus = m_outputNode->pushPacket(packet.get());
            if (!writeStatus) {
                return Result<int>::failure(writeStatus.error());
            }

            ++packetsPushed;
            if (onPacketWritten) {
                onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
            }
        }
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
    Status validateTimestamp(const AVPacket* packet)
    {
        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegVideoEncodedPacketDrainStage validateTimestamp failed: packet is null"));
        }

        if (packet->dts != AV_NOPTS_VALUE &&
            m_lastWrittenDts != AV_NOPTS_VALUE &&
            packet->dts < m_lastWrittenDts) {
            std::ostringstream oss;
            oss << "video packet timestamp moved backwards: current_dts="
                << packet->dts
                << ", last_dts="
                << m_lastWrittenDts;
            return Status::failure(ErrorInfo::internalError(oss.str()));
        }

        return Status::success();
    }

    void updateProgressFromPacket(const AVPacket* packet)
    {
        if (!packet) {
            return;
        }

        ++m_packetCount;

        if (packet->dts != AV_NOPTS_VALUE) {
            m_lastWrittenDts = packet->dts;
        }

        const int64_t timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
        if (timestamp != AV_NOPTS_VALUE) {
            m_lastWrittenOutTimeMs = av_rescale_q(
                timestamp,
                m_outputVideoStream->time_base,
                AVRational{ 1, 1000 }
            );
        }
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
