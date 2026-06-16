#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

struct FFmpegVideoInputMetadata {
    int width = 0;
    int height = 0;
    AVRational sampleAspectRatio{ 1, 1 };
    AVRational timeBase{ 0, 1 };
    AVRational frameRate{ 0, 1 };

    bool hasValidSize() const;

    static FFmpegVideoInputMetadata fromDecoderContextAndStream(
        const AVCodecContext* decoderCtx,
        const AVStream* inputStream);
};

} // namespace media::ffmpeg
