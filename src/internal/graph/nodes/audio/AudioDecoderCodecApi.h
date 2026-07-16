#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

class AudioDecoderCodecApi {
public:
    virtual ~AudioDecoderCodecApi() = default;
    virtual int sendPacket(AVCodecContext* context, const AVPacket* packet) noexcept = 0;
    virtual int receiveFrame(AVCodecContext* context, AVFrame* frame) noexcept = 0;
    virtual void flushBuffers(AVCodecContext* context) noexcept = 0;
};

std::shared_ptr<AudioDecoderCodecApi> makeFFmpegAudioDecoderCodecApi();

} // namespace media::ffmpeg::graph
