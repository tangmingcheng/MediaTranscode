#pragma once

#include "internal/FFmpegUtils.h"

namespace media::ffmpeg {

    AVSampleFormat chooseAudioSampleFormat(const AVCodec* encoder);
    int chooseAudioSampleRate(const AVCodec* encoder, int preferredRate);

    int audioChannelCount(const AVCodecContext* ctx);
    bool ensureAudioDecoderChannelLayout(AVCodecContext* ctx);
    bool copyAudioChannelLayoutToEncoder(AVCodecContext* encoderCtx,
                                         const AVCodecContext* decoderCtx);
    bool setFrameAudioLayoutFromCodecContext(AVFrame* frame,
                                             const AVCodecContext* codecCtx);

#if LIBAVUTIL_VERSION_MAJOR < 57
    int64_t oldAudioChannelLayout(const AVCodecContext* ctx);
#endif

} // namespace media::ffmpeg
