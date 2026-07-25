#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

class MediaVideoDecoderCodecApi {
public:
    virtual ~MediaVideoDecoderCodecApi() = default;
    virtual int sendPacket(AVCodecContext* context, const AVPacket* packet) = 0;
    virtual int receiveFrame(AVCodecContext* context, AVFrame* frame) = 0;
    virtual void flushBuffers(AVCodecContext* context) = 0;
};

std::shared_ptr<MediaVideoDecoderCodecApi> makeMediaVideoDecoderCodecApi();

} // namespace media::ffmpeg::graph
