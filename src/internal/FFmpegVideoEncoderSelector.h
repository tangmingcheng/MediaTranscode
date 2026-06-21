#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    struct VideoEncoderCandidate {
        const AVCodec* encoder = nullptr;
        std::string name;
        AVCodecID codecId = AV_CODEC_ID_NONE;
        AVPixelFormat selectedPixelFormat = AV_PIX_FMT_NONE;
        int score = 0;
        bool hardwareEncoder = false;
        bool experimental = false;
        std::string pixelFormats;
        std::string reason;
    };

    struct VideoEncoderSelection {
        const AVCodec* encoder = nullptr;
        std::string encoderName;
        AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
        std::vector<VideoEncoderCandidate> candidates;
        std::string diagnostic;
    };

    class VideoEncoderSelector {
    public:
        static VideoEncoderSelection select(VideoCodec codec,
                                            bool preferHardwareEncoder = false);
        static AVCodecID codecIdFor(VideoCodec codec);
    };

} // namespace media::ffmpeg
