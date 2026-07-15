#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"

namespace media::ffmpeg::graph {
namespace {

class FfmpegVideoDecoderCodecApi final : public MediaVideoDecoderCodecApi {
public:
    int sendPacket(AVCodecContext* context, const AVPacket* packet) override
    {
        return avcodec_send_packet(context, packet);
    }

    int receiveFrame(AVCodecContext* context, AVFrame* frame) override
    {
        return avcodec_receive_frame(context, frame);
    }

    void flushBuffers(AVCodecContext* context) override
    {
        avcodec_flush_buffers(context);
    }
};

} // namespace

std::shared_ptr<MediaVideoDecoderCodecApi> makeMediaVideoDecoderCodecApi()
{
    return std::make_shared<FfmpegVideoDecoderCodecApi>();
}

} // namespace media::ffmpeg::graph
