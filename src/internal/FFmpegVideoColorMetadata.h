#pragma once

#include <string>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    struct VideoColorMetadata {
        AVRational sampleAspectRatio{ 1, 1 };
        AVColorRange colorRange = AVCOL_RANGE_UNSPECIFIED;
        AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
        AVColorPrimaries colorPrimaries = AVCOL_PRI_UNSPECIFIED;
        AVColorTransferCharacteristic colorTransfer = AVCOL_TRC_UNSPECIFIED;
        AVChromaLocation chromaLocation = AVCHROMA_LOC_UNSPECIFIED;

        static VideoColorMetadata fromFrame(const AVFrame* frame,
                                            AVRational fallbackSampleAspectRatio);
    };

    class VideoColorMetadataUtils {
    public:
        static AVRational sanitizeSampleAspectRatio(AVRational ratio);
        static bool hasSpecifiedColorRange(const VideoColorMetadata& metadata);
        static bool hasSpecifiedColorSpace(const VideoColorMetadata& metadata);
        static bool hasSpecifiedColorPrimaries(const VideoColorMetadata& metadata);
        static bool hasSpecifiedColorTransfer(const VideoColorMetadata& metadata);
        static bool hasSpecifiedChromaLocation(const VideoColorMetadata& metadata);

        static void applyMissingToFrame(AVFrame* frame,
                                        const VideoColorMetadata& metadata);

        static const char* colorRangeName(AVColorRange value);
        static const char* colorSpaceName(AVColorSpace value);
        static const char* colorPrimariesName(AVColorPrimaries value);
        static const char* colorTransferName(AVColorTransferCharacteristic value);
        static const char* chromaLocationName(AVChromaLocation value);
        static std::string describe(const VideoColorMetadata& metadata);
    };

} // namespace media::ffmpeg
