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
            AudioCodec codec = AudioCodec::AAC;
            int audioBitrateKbps = 128;
            bool smartCopy = false;
            std::string diagnostic;
        };

        static Plan plan(const TranscodeConfig& config,
                         const AVStream* inputAudioStream,
                         const AVFormatContext* outputFmtCtx);

        static const char* audioModeName(AudioMode mode);
        static const char* audioCodecName(AudioCodec codec);

    private:
        struct TargetParameters {
            AVCodecID codecId = AV_CODEC_ID_NONE;
            AudioCodec encodeCodec = AudioCodec::AAC;
            int64_t bitRate = 0;
            bool bitRateSpecified = false;
        };

        static TargetParameters resolveTargetParameters(const TranscodeConfig& config,
                                                        const AVStream* inputAudioStream);
        static int plannedBitrateKbps(const TargetParameters& target);
        static AudioCodec audioCodecFromCodecId(AVCodecID codecId);
        static AudioCodec fallbackEncodeCodec(const TranscodeConfig& config,
                                              const AVStream* inputAudioStream);
        static AVCodecID targetCodecId(AudioCodec codec);
        static const char* codecName(AVCodecID codecId);
        static bool bitRateMatches(const AVStream* inputAudioStream,
                                   const TargetParameters& target,
                                   std::string* reason);
        static bool outputContainerSupportsCodec(const AVFormatContext* outputFmtCtx,
                                                 AVCodecID codecId);
        static bool codecRequiresGlobalHeaderForSafeCopy(const AVFormatContext* outputFmtCtx,
                                                         AVCodecID codecId);
        static bool inputHasGlobalHeaderCompatibleExtradata(const AVStream* inputAudioStream);
        static bool canSmartCopy(const TranscodeConfig& config,
                                 const AVStream* inputAudioStream,
                                 const AVFormatContext* outputFmtCtx,
                                 const TargetParameters& target,
                                 std::string* reason);
    };

} // namespace media::ffmpeg
