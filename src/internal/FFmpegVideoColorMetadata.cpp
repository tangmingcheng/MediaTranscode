#include "internal/FFmpegVideoColorMetadata.h"

#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    bool isValidRatio(AVRational ratio)
    {
        return ratio.num > 0 && ratio.den > 0;
    }

    const char* fallbackName(const char* name)
    {
        return name ? name : "unknown";
    }

} // namespace

    VideoColorMetadata VideoColorMetadata::fromFrame(const AVFrame* frame,
                                                     AVRational fallbackSampleAspectRatio)
    {
        VideoColorMetadata metadata;
        metadata.sampleAspectRatio = frame && isValidRatio(frame->sample_aspect_ratio)
            ? frame->sample_aspect_ratio
            : VideoColorMetadataUtils::sanitizeSampleAspectRatio(fallbackSampleAspectRatio);

        if (!frame) {
            return metadata;
        }

        metadata.colorRange = frame->color_range;
        metadata.colorSpace = frame->colorspace;
        metadata.colorPrimaries = frame->color_primaries;
        metadata.colorTransfer = frame->color_trc;
        metadata.chromaLocation = frame->chroma_location;
        return metadata;
    }

    AVRational VideoColorMetadataUtils::sanitizeSampleAspectRatio(AVRational ratio)
    {
        return isValidRatio(ratio) ? ratio : AVRational{ 1, 1 };
    }

    bool VideoColorMetadataUtils::hasSpecifiedColorRange(const VideoColorMetadata& metadata)
    {
        return metadata.colorRange != AVCOL_RANGE_UNSPECIFIED;
    }

    bool VideoColorMetadataUtils::hasSpecifiedColorSpace(const VideoColorMetadata& metadata)
    {
        return metadata.colorSpace != AVCOL_SPC_UNSPECIFIED;
    }

    bool VideoColorMetadataUtils::hasSpecifiedColorPrimaries(const VideoColorMetadata& metadata)
    {
        return metadata.colorPrimaries != AVCOL_PRI_UNSPECIFIED;
    }

    bool VideoColorMetadataUtils::hasSpecifiedColorTransfer(const VideoColorMetadata& metadata)
    {
        return metadata.colorTransfer != AVCOL_TRC_UNSPECIFIED;
    }

    bool VideoColorMetadataUtils::hasSpecifiedChromaLocation(const VideoColorMetadata& metadata)
    {
        return metadata.chromaLocation != AVCHROMA_LOC_UNSPECIFIED;
    }

    void VideoColorMetadataUtils::applyMissingToFrame(AVFrame* frame,
                                                      const VideoColorMetadata& metadata)
    {
        if (!frame) {
            return;
        }

        if (!isValidRatio(frame->sample_aspect_ratio)) {
            frame->sample_aspect_ratio = sanitizeSampleAspectRatio(metadata.sampleAspectRatio);
        }

        if (frame->color_range == AVCOL_RANGE_UNSPECIFIED && hasSpecifiedColorRange(metadata)) {
            frame->color_range = metadata.colorRange;
        }

        if (frame->colorspace == AVCOL_SPC_UNSPECIFIED && hasSpecifiedColorSpace(metadata)) {
            frame->colorspace = metadata.colorSpace;
        }

        if (frame->color_primaries == AVCOL_PRI_UNSPECIFIED && hasSpecifiedColorPrimaries(metadata)) {
            frame->color_primaries = metadata.colorPrimaries;
        }

        if (frame->color_trc == AVCOL_TRC_UNSPECIFIED && hasSpecifiedColorTransfer(metadata)) {
            frame->color_trc = metadata.colorTransfer;
        }

        if (frame->chroma_location == AVCHROMA_LOC_UNSPECIFIED && hasSpecifiedChromaLocation(metadata)) {
            frame->chroma_location = metadata.chromaLocation;
        }
    }

    const char* VideoColorMetadataUtils::colorRangeName(AVColorRange value)
    {
        return fallbackName(av_color_range_name(value));
    }

    const char* VideoColorMetadataUtils::colorSpaceName(AVColorSpace value)
    {
        return fallbackName(av_color_space_name(value));
    }

    const char* VideoColorMetadataUtils::colorPrimariesName(AVColorPrimaries value)
    {
        return fallbackName(av_color_primaries_name(value));
    }

    const char* VideoColorMetadataUtils::colorTransferName(AVColorTransferCharacteristic value)
    {
        return fallbackName(av_color_transfer_name(value));
    }

    const char* VideoColorMetadataUtils::chromaLocationName(AVChromaLocation value)
    {
        return fallbackName(av_chroma_location_name(value));
    }

    std::string VideoColorMetadataUtils::describe(const VideoColorMetadata& metadata)
    {
        std::ostringstream oss;
        oss << "range=" << colorRangeName(metadata.colorRange)
            << ", space=" << colorSpaceName(metadata.colorSpace)
            << ", primaries=" << colorPrimariesName(metadata.colorPrimaries)
            << ", transfer=" << colorTransferName(metadata.colorTransfer)
            << ", chroma=" << chromaLocationName(metadata.chromaLocation)
            << ", sar=" << metadata.sampleAspectRatio.num << "/" << metadata.sampleAspectRatio.den;
        return oss.str();
    }

} // namespace media::ffmpeg
