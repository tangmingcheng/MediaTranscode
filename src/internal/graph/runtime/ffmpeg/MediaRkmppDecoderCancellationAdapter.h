#pragma once
#include "media_transcode/Result.h"
struct AVCodecContext;
namespace media::ffmpeg::graph {
class MediaVideoDecoderCallbackState;
class MediaRkmppDecoderCancellationAdapter final {
public:
    static ::media::Status registerCallback(AVCodecContext* context,
        MediaVideoDecoderCallbackState& owner);
};
} // namespace media::ffmpeg::graph
