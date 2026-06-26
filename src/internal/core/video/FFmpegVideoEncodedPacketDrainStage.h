#pragma once

#include "internal/FFmpegError.h"
#include "internal/output/PacketOutputNode.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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
        m_lastWrittenOutTimeMs = 0;
        m_lastWrittenDts = AV_NOPTS_VALUE;
    }

    Status initialize(const Config& config)
    {
        reset();

        if (!config.encoderCtx || !config.outputVideoStream || !config.outputNode) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegVideoEncodedPacketDrainStage initialize failed: invalid config"));
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
                "DrainStage sendFrame failed"));
        }

        const int ret = avcodec_send_frame(m_encoderCtx, frame);
        if (ret < 0) {
            return Status::failure(makeFFmpegError("avcodec_send_frame failed", ret));
        }

        return Status::success();
    }

    Result<int> receiveAndPushPackets(const PacketWrittenCallback& cb = {})
    {
        if (!m_encoderCtx || !m_outputVideoStream || !m_outputNode) {
            return Result<int>::failure(ErrorInfo::notInitialized(
                "DrainStage not initialized"));
        }

        int count = 0;

        while (true) {
            PacketPtr pkt = makePacket();
            if (!pkt) {
                return Result<int>::failure(makeAllocationError("packet alloc failed"));
            }

            int ret = avcodec_receive_packet(m_encoderCtx, pkt.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }

            if (ret < 0) {
                return Result<int>::failure(makeFFmpegError("receive_packet failed", ret));
            }

            pkt->stream_index = m_outputVideoStream->index;

            av_packet_rescale_ts(pkt.get(), m_encoderCtx->time_base, m_outputVideoStream->time_base);

            const Status st = m_outputNode->pushPacket(pkt.get());
            if (!st) {
                return Result<int>::failure(st.error());
            }

            m_packetCount++;
            count++;

            if (cb) {
                cb(m_packetCount, m_lastWrittenOutTimeMs);
            }
        }

        return Result<int>::success(count);
    }

private:
    AVCodecContext* m_encoderCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;
    PacketOutputNode* m_outputNode = nullptr;

    int64_t m_packetCount = 0;
    int64_t m_lastWrittenOutTimeMs = 0;
    int64_t m_lastWrittenDts = AV_NOPTS_VALUE;
};

} // namespace media::ffmpeg
