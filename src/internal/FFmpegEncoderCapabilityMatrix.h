#pragma once

#include "internal/TranscodeTypes.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg {

    enum class FFmpegEncoderFamily {
        Unknown,
        X264,
        X265,
        NVENC,
        RKMPP,
        MediaFoundation,
        QSV,
        VAAPI,
        AMF,
        Generic
    };

    struct FFmpegEncoderCapabilities {
        std::string encoderName;
        FFmpegEncoderFamily family = FFmpegEncoderFamily::Unknown;
        std::string familyName = "unknown";
        bool hardware = false;

        bool supportsBitRateField = true;
        bool supportsMinRateField = true;
        bool supportsMaxRateField = true;
        bool supportsBufferSizeField = true;
        bool supportsPrivateVbvOptions = true;

        bool supportsCbr = true;
        bool supportsVbr = true;
        bool supportsCrf = false;
        bool supportsCappedVbr = true;

        // Common FFmpeg private option names. Empty means the family should not emit that option.
        std::string rateControlOptionName;
        std::string qualityOptionName;
        bool qualityOptionInteger = true;
        std::string presetOptionName;

        // x264/x265 CBR is commonly represented as nal-hrd=cbr rather than rc=cbr.
        bool supportsNalHrdCbr = false;
    };

    class FFmpegEncoderCapabilityMatrix {
    public:
        static FFmpegEncoderCapabilities query(const AVCodec* encoder);

        static bool supportsRateControl(const FFmpegEncoderCapabilities& capabilities,
                                        VideoRateControlMode mode);

        static std::string rateControlValue(const FFmpegEncoderCapabilities& capabilities,
                                            VideoRateControlMode mode);

        static std::string presetValue(const FFmpegEncoderCapabilities& capabilities,
                                       VideoSpeedPreset preset);

        static std::string familyName(FFmpegEncoderFamily family);
        static std::string rateControlName(VideoRateControlMode mode);
        static bool isRateControlPrivateOption(const std::string& name);
    };

} // namespace media::ffmpeg
