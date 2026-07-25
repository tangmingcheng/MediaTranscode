#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

class MediaVideoEncoderCodecApi {
public:
    virtual ~MediaVideoEncoderCodecApi() = default;
    virtual int sendFrame(AVCodecContext* context, const AVFrame* frame) = 0;
    virtual int receivePacket(AVCodecContext* context, AVPacket* packet) = 0;
    virtual void flushBuffers(AVCodecContext* context) = 0;
};

std::shared_ptr<MediaVideoEncoderCodecApi> makeMediaVideoEncoderCodecApi();

} // namespace media::ffmpeg::graph
