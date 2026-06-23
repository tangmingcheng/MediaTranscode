#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <cstdint>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
}

namespace media::ffmpeg {

    std::string errorString(int err);

    const char* preferredAudioEncoderName(AudioCodec codec);
    AVCodecID fallbackAudioCodecId(AudioCodec codec);

} // namespace media::ffmpeg
