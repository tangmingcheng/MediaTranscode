#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

    class FFmpegAudioStrategyPlanner {
    public:
        struct Plan {
            AudioMode mode = AudioMode::None;
            bool smartCopy = false;
            std::string diagnostic;
        };

        static Plan plan(const TranscodeConfig& config,
                         const AVStream* inputAudioStream,
                         const AVFormatContext* outputFmtCtx);

        static const char* audioModeName(AudioMode mode);
        static const char* audioCodecName(AudioCodec codec);

    private:
        static AVCodecID targetCodecId(AudioCodec codec);
        static const char* codecName(AVCodecID codecId);
        static bool outputContainerSupportsCodec(const AVFormatContext* outputFmtCtx,
                                                 AVCodecID codecId);
        static bool codecRequiresGlobalHeaderForSafeCopy(const AVFormatContext* outputFmtCtx,
                                                         AVCodecID codecId);
        static bool inputHasGlobalHeaderCompatibleExtradata(const AVStream* inputAudioStream);
        static bool canSmartCopy(const TranscodeConfig& config,
                                 const AVStream* inputAudioStream,
                                 const AVFormatContext* outputFmtCtx,
                                 std::string* reason);
    };

} // namespace media::ffmpeg
