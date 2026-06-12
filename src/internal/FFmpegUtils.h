#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
}

namespace media::ffmpeg {

    std::string errorString(int err);

    std::vector<const char*> videoEncoderCandidateNames(VideoCodec codec);
    const char* preferredVideoEncoderName(VideoCodec codec);
    AVCodecID fallbackVideoCodecId(VideoCodec codec);

    const char* preferredAudioEncoderName(AudioCodec codec);
    AVCodecID fallbackAudioCodecId(AudioCodec codec);

    int normalizeEvenSize(int value);
    int chooseOutputFps(const TranscodeConfig& config, const AVStream* inputVideoStream);

    AVPixelFormat chooseVideoEncoderPixelFormat(const AVCodec* encoder);
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

    bool isHardwareEncoderName(const char* name);
    void setVideoEncoderOptions(AVCodecContext* encoderCtx, const AVCodec* encoder);

} // namespace media::ffmpeg
