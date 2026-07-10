#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegPacedAvio.h"

#include <cstring>
#include <memory>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
}

namespace media::ffmpeg {

inline int interruptImmediately(void*) noexcept
{
    return 1;
}

inline bool equalsFormatName(const AVInputFormat* format, std::string_view name) noexcept
{
    return format && format->name && name == format->name;
}

inline bool startsWith(std::string_view value, std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

inline bool isInterruptibleInputContext(const AVFormatContext* ctx) noexcept
{
    if (!ctx) {
        return false;
    }

    if (equalsFormatName(ctx->iformat, "sdp") ||
        equalsFormatName(ctx->iformat, "rtsp")) {
        return true;
    }

    const char* url = ctx->url;
    if (!url) {
        return false;
    }

    const std::string_view inputUrl(url, std::strlen(url));
    return startsWith(inputUrl, "rtp://") ||
           startsWith(inputUrl, "udp://") ||
           startsWith(inputUrl, "rtsp://");
}

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

struct CodecParametersDeleter {
    void operator()(AVCodecParameters* params) const noexcept
    {
        if (params) {
            avcodec_parameters_free(&params);
        }
    }
};

struct InputFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const noexcept
    {
        if (!ctx) {
            return;
        }

        if (isInterruptibleInputContext(ctx)) {
            ctx->interrupt_callback.callback = interruptImmediately;
            ctx->interrupt_callback.opaque = nullptr;
        }

        AVFormatContext* tmp = ctx;
        avformat_close_input(&tmp);
    }
};

struct OutputFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const noexcept
    {
        if (!ctx) {
            return;
        }

        if (ctx->oformat && !(ctx->oformat->flags & AVFMT_NOFILE) && ctx->pb) {
            if (isPacedWriteAvio(ctx->pb)) {
                closePacedWriteAvio(&ctx->pb);
            } else {
                avio_closep(&ctx->pb);
            }
        }

        avformat_free_context(ctx);
    }
};

struct FilterGraphDeleter {
    void operator()(AVFilterGraph* graph) const noexcept
    {
        if (graph) {
            avfilter_graph_free(&graph);
        }
    }
};

struct FilterInOutDeleter {
    void operator()(AVFilterInOut* inout) const noexcept
    {
        if (inout) {
            avfilter_inout_free(&inout);
        }
    }
};

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const noexcept
    {
        if (ctx) {
            swr_free(&ctx);
        }
    }
};

struct BufferRefDeleter {
    void operator()(AVBufferRef* ref) const noexcept
    {
        if (ref) {
            av_buffer_unref(&ref);
        }
    }
};

struct AudioFifoDeleter {
    void operator()(AVAudioFifo* fifo) const noexcept
    {
        if (fifo) {
            av_audio_fifo_free(fifo);
        }
    }
};

struct BufferSrcParametersDeleter {
    void operator()(AVBufferSrcParameters* params) const noexcept
    {
        if (!params) {
            return;
        }

        if (params->hw_frames_ctx) {
            av_buffer_unref(&params->hw_frames_ctx);
        }

        av_freep(&params);
    }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using CodecParametersPtr = std::unique_ptr<AVCodecParameters, CodecParametersDeleter>;
using InputFormatContextPtr = std::unique_ptr<AVFormatContext, InputFormatContextDeleter>;
using OutputFormatContextPtr = std::unique_ptr<AVFormatContext, OutputFormatContextDeleter>;
using FilterGraphPtr = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;
using FilterInOutPtr = std::unique_ptr<AVFilterInOut, FilterInOutDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using BufferRefPtr = std::unique_ptr<AVBufferRef, BufferRefDeleter>;
using AudioFifoPtr = std::unique_ptr<AVAudioFifo, AudioFifoDeleter>;
using BufferSrcParametersPtr = std::unique_ptr<AVBufferSrcParameters, BufferSrcParametersDeleter>;

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

inline CodecParametersPtr makeCodecParameters()
{
    return CodecParametersPtr(avcodec_parameters_alloc());
}

inline FilterGraphPtr makeFilterGraph()
{
    return FilterGraphPtr(avfilter_graph_alloc());
}

inline FilterInOutPtr makeFilterInOut()
{
    return FilterInOutPtr(avfilter_inout_alloc());
}

inline AudioFifoPtr makeAudioFifo(AVSampleFormat sampleFormat, int channels, int initialSamples)
{
    return AudioFifoPtr(av_audio_fifo_alloc(sampleFormat, channels, initialSamples));
}

inline BufferRefPtr makeBufferRef(AVBufferRef* ref)
{
    return BufferRefPtr(ref ? av_buffer_ref(ref) : nullptr);
}

inline BufferSrcParametersPtr makeBufferSrcParameters()
{
    return BufferSrcParametersPtr(av_buffersrc_parameters_alloc());
}

} // namespace media::ffmpeg
