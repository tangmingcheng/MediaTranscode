#include "internal/FFmpegVideoEncodeOptionsApplier.h"

#include "internal/FFmpegEncoderCapabilityMatrix.h"
#include "internal/FFmpegVideoBitrateControlPlanner.h"
#include "internal/FFmpegVideoBitrateOptionAdapter.h"

#include <algorithm>
#include <sstream>
#include <string>

extern "C" {
#include <libavutil/opt.h>
}

namespace media::ffmpeg {
namespace {

    int chooseGopSize(const VideoEncodeOptions& options, int outputFps)
    {
        if (options.gopSize > 0) {
            return options.gopSize;
        }

        return std::max(10, outputFps * 2);
    }

    int chooseMaxBFrames(const VideoEncodeOptions& options)
    {
        return std::max(0, options.maxBFrames);
    }

    bool hasOption(void* target, const char* optionName)
    {
        if (!target || !optionName || !*optionName) {
            return false;
        }

        return av_opt_find(
            target,
            optionName,
            nullptr,
            0,
            AV_OPT_SEARCH_CHILDREN
        ) != nullptr;
    }

    bool setStringOptionIfSupported(void* target,
                                    const char* optionName,
                                    const std::string& value,
                                    VideoEncodeOptionsApplyReport& report)
    {
        if (value.empty()) {
            return false;
        }

        if (!optionName || !*optionName) {
            return false;
        }

        if (!hasOption(target, optionName)) {
            report.unsupportedOptions.emplace_back(optionName);
            return false;
        }

        const int ret = av_opt_set(
            target,
            optionName,
            value.c_str(),
            AV_OPT_SEARCH_CHILDREN
        );

        if (ret >= 0) {
            report.appliedOptions.emplace_back(std::string(optionName) + "=" + value);
            return true;
        }

        report.failedOptions.emplace_back(std::string(optionName) + "=" + value);
        return false;
    }

    bool setIntegerOptionIfSupported(void* target,
                                     const char* optionName,
                                     int64_t value,
                                     VideoEncodeOptionsApplyReport& report)
    {
        if (!hasOption(target, optionName)) {
            report.unsupportedOptions.emplace_back(optionName);
            return false;
        }

        const int ret = av_opt_set_int(
            target,
            optionName,
            value,
            AV_OPT_SEARCH_CHILDREN
        );

        if (ret >= 0) {
            report.appliedOptions.emplace_back(std::string(optionName) + "=" + std::to_string(value));
            return true;
        }

        report.failedOptions.emplace_back(std::string(optionName) + "=" + std::to_string(value));
        return false;
    }

    void applyOptionalStringOptions(AVCodecContext* encoderContext,
                                    const AVCodec* encoder,
                                    const VideoEncodeOptions& options,
                                    VideoEncodeOptionsApplyReport& report)
    {
        const FFmpegEncoderCapabilities capabilities = FFmpegEncoderCapabilityMatrix::query(encoder);

        const std::string resolvedPresetValue = !options.preset.empty()
            ? options.preset
            : FFmpegEncoderCapabilityMatrix::presetValue(capabilities, options.speedPreset);

        const std::string resolvedPresetOption = !capabilities.presetOptionName.empty()
            ? capabilities.presetOptionName
            : (!options.preset.empty() ? "preset" : "");

        report.presetApplied = setStringOptionIfSupported(
            encoderContext,
            resolvedPresetOption.c_str(),
            resolvedPresetValue,
            report
        );

        report.tuneApplied = setStringOptionIfSupported(
            encoderContext,
            "tune",
            options.tune,
            report
        );

        report.profileApplied = setStringOptionIfSupported(
            encoderContext,
            "profile",
            options.profile,
            report
        );

        report.levelApplied = setStringOptionIfSupported(
            encoderContext,
            "level",
            options.level,
            report
        );
    }

    void applyBitrateOptions(AVCodecContext* encoderContext,
                             const VideoBitrateOptionPlan& optionPlan,
                             VideoEncodeOptionsApplyReport& report)
    {
        if (optionPlan.bitRate > 0) {
            encoderContext->bit_rate = optionPlan.bitRate;
            report.bitRate = optionPlan.bitRate;
        }

        if (optionPlan.minBitRate > 0) {
            encoderContext->rc_min_rate = static_cast<int>(optionPlan.minBitRate);
            report.minBitRate = optionPlan.minBitRate;
        }

        if (optionPlan.maxBitRate > 0) {
            encoderContext->rc_max_rate = static_cast<int>(optionPlan.maxBitRate);
            report.maxBitRate = optionPlan.maxBitRate;
        }

        if (optionPlan.bufferSize > 0) {
            encoderContext->rc_buffer_size = static_cast<int>(optionPlan.bufferSize);
            report.bufferSize = optionPlan.bufferSize;
        }

        for (const VideoBitrateOption& option : optionPlan.privateOptions) {
            bool applied = false;

            switch (option.type) {
            case VideoBitrateOption::Type::String:
                applied = setStringOptionIfSupported(
                    encoderContext,
                    option.name.c_str(),
                    option.stringValue,
                    report
                );
                break;

            case VideoBitrateOption::Type::Integer:
                applied = setIntegerOptionIfSupported(
                    encoderContext,
                    option.name.c_str(),
                    option.integerValue,
                    report
                );
                break;
            }

            if (applied && FFmpegEncoderCapabilityMatrix::isRateControlPrivateOption(option.name)) {
                report.rateControlApplied = true;
            }
        }
    }

    std::string joinList(const std::vector<std::string>& values)
    {
        if (values.empty()) {
            return "none";
        }

        std::ostringstream oss;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                oss << ",";
            }
            oss << values[i];
        }

        return oss.str();
    }

} // namespace

    VideoEncodeOptionsApplyReport VideoEncodeOptionsApplier::apply(AVCodecContext* encoderContext,
                                                                    const AVCodec* encoder,
                                                                    const TranscodeConfig& config,
                                                                    int outputFps)
    {
        VideoEncodeOptionsApplyReport report;

        if (!encoderContext) {
            return report;
        }

        encoderContext->gop_size = chooseGopSize(config.videoEncode, outputFps);
        encoderContext->max_b_frames = chooseMaxBFrames(config.videoEncode);

        report.gopSize = encoderContext->gop_size;
        report.maxBFrames = encoderContext->max_b_frames;

        const VideoBitratePlan bitratePlan = VideoBitrateControlPlanner::plan(
            config,
            config.width,
            config.height,
            outputFps
        );

        const VideoBitrateOptionPlan optionPlan = VideoBitrateOptionAdapter::adapt(
            encoder,
            bitratePlan
        );

        applyBitrateOptions(encoderContext, optionPlan, report);
        applyOptionalStringOptions(encoderContext, encoder, config.videoEncode, report);

        return report;
    }

    std::string VideoEncodeOptionsApplyReport::describe() const
    {
        std::ostringstream oss;
        oss << "gop=" << gopSize
            << ", bframes=" << maxBFrames
            << ", bitrate=" << bitRate
            << ", min_bitrate=" << minBitRate
            << ", max_bitrate=" << maxBitRate
            << ", buffer=" << bufferSize
            << ", preset=" << (presetApplied ? "applied" : "default")
            << ", tune=" << (tuneApplied ? "applied" : "default")
            << ", profile=" << (profileApplied ? "applied" : "default")
            << ", level=" << (levelApplied ? "applied" : "default")
            << ", rc_option=" << (rateControlApplied ? "applied" : "default")
            << ", applied=[" << joinList(appliedOptions) << "]"
            << ", unsupported=[" << joinList(unsupportedOptions) << "]"
            << ", failed=[" << joinList(failedOptions) << "]";

        return oss.str();
    }

} // namespace media::ffmpeg
