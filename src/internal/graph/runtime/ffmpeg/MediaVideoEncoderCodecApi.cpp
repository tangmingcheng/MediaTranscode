#include "internal/graph/runtime/ffmpeg/MediaVideoEncoderCodecApi.h"

namespace media::ffmpeg::graph {
namespace {

class FfmpegVideoEncoderCodecApi final : public MediaVideoEncoderCodecApi {
public:
    int sendFrame(AVCodecContext* context, const AVFrame* frame) override
    {
        return avcodec_send_frame(context, frame);
    }

    int receivePacket(AVCodecContext* context, AVPacket* packet) override
    {
        return avcodec_receive_packet(context, packet);
    }

    void flushBuffers(AVCodecContext* context) override
    {
        avcodec_flush_buffers(context);
    }
};

} // namespace

std::shared_ptr<MediaVideoEncoderCodecApi> makeMediaVideoEncoderCodecApi()
{
    return std::make_shared<FfmpegVideoEncoderCodecApi>();
}

} // namespace media::ffmpeg::graph
