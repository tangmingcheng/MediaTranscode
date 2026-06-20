#include "internal/FFmpegBufferSourceMetadataApplier.h"

#include <initializer_list>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

namespace media::ffmpeg {
namespace {

    bool isUnsupportedOptionError(int ret)
    {
        return ret == AVERROR_OPTION_NOT_FOUND;
    }

    bool setIntOptionOnTarget(void* target, const char* optionName, int value, int flags)
    {
        if (!target || !optionName) {
            return false;
        }

        const int ret = av_opt_set_int(target, optionName, value, flags);
        return ret >= 0;
    }

    bool setIntOption(AVFilterContext* context, const char* optionName, int value)
    {
        if (!context || !optionName) {
            return false;
        }

        const int ret = av_opt_set_int(
            context,
            optionName,
            value,
            AV_OPT_SEARCH_CHILDREN
        );

        if (ret >= 0) {
            return true;
        }

        if (!isUnsupportedOptionError(ret)) {
            // Some FFmpeg versions expose buffer source private options only on
            // priv; retry there before treating the option as unavailable.
            return setIntOptionOnTarget(context->priv, optionName, value, 0);
        }

        return setIntOptionOnTarget(context->priv, optionName, value, 0);
    }

    bool setFirstSupportedIntOption(AVFilterContext* context,
                                    std::initializer_list<const char*> optionNames,
                                    int value)
    {
        for (const char* optionName : optionNames) {
            if (setIntOption(context, optionName, value)) {
                return true;
            }
        }

        return false;
    }

} // namespace

    bool BufferSourceMetadataApplyReport::anyApplied() const
    {
        return colorRangeApplied ||
            colorSpaceApplied ||
            colorPrimariesApplied ||
            colorTransferApplied ||
            chromaLocationApplied;
    }

    BufferSourceMetadataApplyReport BufferSourceMetadataApplier::apply(
        AVFilterContext* bufferSourceContext,
        const VideoColorMetadata& metadata)
    {
        BufferSourceMetadataApplyReport report;

        if (!bufferSourceContext) {
            return report;
        }

        if (VideoColorMetadataUtils::hasSpecifiedColorRange(metadata)) {
            report.colorRangeApplied = setFirstSupportedIntOption(
                bufferSourceContext,
                { "range", "color_range" },
                static_cast<int>(metadata.colorRange)
            );
        }

        if (VideoColorMetadataUtils::hasSpecifiedColorSpace(metadata)) {
            report.colorSpaceApplied = setFirstSupportedIntOption(
                bufferSourceContext,
                { "colorspace", "color_space" },
                static_cast<int>(metadata.colorSpace)
            );
        }

        if (VideoColorMetadataUtils::hasSpecifiedColorPrimaries(metadata)) {
            report.colorPrimariesApplied = setFirstSupportedIntOption(
                bufferSourceContext,
                { "color_primaries", "primaries" },
                static_cast<int>(metadata.colorPrimaries)
            );
        }

        if (VideoColorMetadataUtils::hasSpecifiedColorTransfer(metadata)) {
            report.colorTransferApplied = setFirstSupportedIntOption(
                bufferSourceContext,
                { "color_trc", "color_transfer", "transfer" },
                static_cast<int>(metadata.colorTransfer)
            );
        }

        if (VideoColorMetadataUtils::hasSpecifiedChromaLocation(metadata)) {
            report.chromaLocationApplied = setFirstSupportedIntOption(
                bufferSourceContext,
                { "chroma_location" },
                static_cast<int>(metadata.chromaLocation)
            );
        }

        return report;
    }

} // namespace media::ffmpeg
