#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

class AudioEncoderCodecApi {
public:
    virtual ~AudioEncoderCodecApi() = default;
    virtual int sendFrame(AVCodecContext* context, const AVFrame* frame) noexcept = 0;
    virtual int receivePacket(AVCodecContext* context, AVPacket* packet) noexcept = 0;
};

std::shared_ptr<AudioEncoderCodecApi> makeFFmpegAudioEncoderCodecApi();

} // namespace media::ffmpeg::graph
