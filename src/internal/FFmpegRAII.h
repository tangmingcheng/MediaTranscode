#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

struct FrameDeleter {
    void operator()(AVFrame* frame) const noexcept
    {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const noexcept
    {
        if (packet) {
            av_packet_free(&packet);
        }
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept
    {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;

inline FramePtr makeFrame()
{
    return FramePtr(av_frame_alloc());
}

inline PacketPtr makePacket()
{
    return PacketPtr(av_packet_alloc());
}

inline CodecContextPtr makeCodecContext(const AVCodec* codec)
{
    return CodecContextPtr(avcodec_alloc_context3(codec));
}

} // namespace media::ffmpeg
