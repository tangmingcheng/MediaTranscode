#include "internal/graph/nodes/audio/AudioEncoderCodecApi.h"

namespace media::ffmpeg::graph {
namespace {

class FFmpegAudioEncoderCodecApi final : public AudioEncoderCodecApi {
public:
    int sendFrame(AVCodecContext* context, const AVFrame* frame) noexcept override
    {
        return avcodec_send_frame(context, frame);
    }

    int receivePacket(AVCodecContext* context, AVPacket* packet) noexcept override
    {
        return avcodec_receive_packet(context, packet);
    }
};

} // namespace

std::shared_ptr<AudioEncoderCodecApi> makeFFmpegAudioEncoderCodecApi()
{
    return std::make_shared<FFmpegAudioEncoderCodecApi>();
}

} // namespace media::ffmpeg::graph
