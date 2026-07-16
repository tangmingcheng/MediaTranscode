#include "internal/graph/nodes/audio/AudioDecoderCodecApi.h"

namespace media::ffmpeg::graph {
namespace {

class FFmpegAudioDecoderCodecApi final : public AudioDecoderCodecApi {
public:
    int sendPacket(AVCodecContext* context, const AVPacket* packet) noexcept override
    {
        return avcodec_send_packet(context, packet);
    }

    int receiveFrame(AVCodecContext* context, AVFrame* frame) noexcept override
    {
        return avcodec_receive_frame(context, frame);
    }

    void flushBuffers(AVCodecContext* context) noexcept override
    {
        avcodec_flush_buffers(context);
    }
};

} // namespace

std::shared_ptr<AudioDecoderCodecApi> makeFFmpegAudioDecoderCodecApi()
{
    return std::make_shared<FFmpegAudioDecoderCodecApi>();
}

} // namespace media::ffmpeg::graph
