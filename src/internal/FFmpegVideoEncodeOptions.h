#pragma once

#include "internal/TranscodeTypes.h"

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg {

    enum class VideoEncoderFamily {
        Generic,
        LibX264,
        LibX265,
        NVENC,
        QSV,
        AMF,
        MediaFoundation,
        RockchipMPP,
        VideoToolbox,
        VPX,
        AOM,
        SVTAV1,
        Rav1e
    };

    struct VideoEncoderPrivateOption {
        std::string key;
        std::string value;
    };

    struct VideoEncodeOptionPlan {
        VideoEncoderFamily encoderFamily = VideoEncoderFamily::Generic;
        VideoRateControlMode rateControl = VideoRateControlMode::Auto;

        int64_t bitRate = 0;
        int64_t maxBitRate = 0;
        int64_t bufferSize = 0;
        int gopSize = 0;
        int maxBFrames = 0;

        std::string preset;
        std::string tune;
        std::string profile;
        std::string level;
        std::vector<VideoEncoderPrivateOption> privateOptions;

        std::string description;
    };

    struct VideoEncodeOptionApplyReport {
        bool contextFieldsApplied = false;
        std::vector<VideoEncoderPrivateOption> appliedPrivateOptions;
        std::vector<VideoEncoderPrivateOption> ignoredPrivateOptions;

        bool hasIgnoredOptions() const;
        std::string describe() const;
    };

    class VideoEncoderFamilyClassifier {
    public:
        static VideoEncoderFamily classify(const AVCodec* encoder);
        static const char* name(VideoEncoderFamily family);
    };

    class VideoEncodeOptionsPlanner {
    public:
        static VideoEncodeOptionPlan build(const TranscodeConfig& config,
                                           const AVCodec* encoder,
                                           int outputFps);
    };

    class VideoEncodeOptionsApplier {
    public:
        static VideoEncodeOptionApplyReport apply(AVCodecContext* encoderCtx,
                                                  const AVCodec* encoder,
                                                  const VideoEncodeOptionPlan& plan);
    };

} // namespace media::ffmpeg
