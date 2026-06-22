#include "internal/FFmpegVideoEncodeOptionsApplier.h"

#include "internal/FFmpegVideoBitrateControlPlanner.h"
#include "internal/FFmpegVideoBitrateOptionAdapter.h"

#include <algorithm>
#include <sstream>
#include <string>

extern "C" {
#include <libavutil/avstring.h>
#include <libavutil/error.h>
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

    bool encoderNameContains(const AVCodec* encoder, const char* token)
    {
        return encoder && encoder->name && std::string(encoder->name).find(token) != std::string::npos;
    }

    bool encoderNameEquals(const AVCodec* encoder, const char* name)
    {
        return encoder && encoder->name && std::string(encoder->name) == name;
    }

    bool isRateControlPrivateOption(const std::string& name)
    {
        return name == "rc" || name == "rate_control" || name == "rc_mode" || name == "nal-hrd";
    }

    std::string nativePresetForSpeed(const AVCodec* encoder,
                                     VideoEncodeSpeedPreset speedPreset)
    {
        if (speedPreset == VideoEncodeSpeedPreset::Auto || !encoder || !encoder->name) {
            return {};
        }

        if (encoderNameContains(encoder, "_nvenc")) {
            switch (speedPreset) {
            case VideoEncodeSpeedPreset::Fast:
                return "p1";
            case VideoEncodeSpeedPreset::Balanced:
                return "p4";
            case VideoEncodeSpeedPreset::Quality:
                return "p7";
            case VideoEncodeSpeedPreset::Auto:
            default:
                return {};
            }
        }

        if (encoderNameEquals(encoder, "libx264") || encoderNameEquals(encoder, "libx265")) {
            switch (speedPreset) {
            case VideoEncodeSpeedPreset::Fast:
                return "fast";
            case VideoEncodeSpeedPreset::Balanced:
                return "medium";
            case VideoEncodeSpeedPreset::Quality:
                return "slow";
            case VideoEncodeSpeedPreset::Auto:
            default:
                return {};
            }
        }

        return {};
    }

    void applyOptionalStringOptions(AVCodecContext* encoderContext,
                                    const AVCodec* encoder,
                                    const VideoEncodeOptions& options,
                                    VideoEncodeOptionsApplyReport& report)
    {
        const std::string resolvedPreset = !options.preset.empty()
            ? options.preset
            : nativePresetForSpeed(encoder, options.speedPreset);

        report.presetApplied = setStringOptionIfSupported(
            encoderContext,
            "preset",
            resolvedPreset,
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

    void applyBitrateOptionPlan(AVCodecContext* encoderContext,
                                const VideoBitrateOptionPlan& optionPlan,
                                VideoEncodeOptionsApplyReport& report)
    {
        if (optionPlan.bitRate > 0) {
            encoderContext->bit_rate = optionPlan.bitRate;
            report.bitRate = encoderContext->bit_rate;
        }

        if (optionPlan.minBitRate > 0) {
            encoderContext->rc_min_rate = optionPlan.minBitRate;
            report.minBitRate = encoderContext->rc_min_rate;
        }

        if (optionPlan.maxBitRate > 0) {
            encoderContext->rc_max_rate = optionPlan.maxBitRate;
            report.maxBitRate = encoderContext->rc_max_rate;
        }

        if (optionPlan.bufferSize > 0) {
            encoderContext->rc_buffer_size = static_cast<int>(optionPlan.bufferSize);
            report.bufferSize = encoderContext->rc_buffer_size;
        }

        for (const VideoBitrateOption& option : optionPlan.privateOptions) {
            bool applied = false;
            if (option.type == VideoBitrateOption::Type::String) {
                applied = setStringOptionIfSupported(
                    encoderContext,
                    option.name.c_str(),
                    option.stringValue,
                    report
                );
            }
            else {
                applied = setIntegerOptionIfSupported(
                    encoderContext,
                    option.name.c_str(),
                    option.integerValue,
                    report
                );
            }

            if (applied && isRateControlPrivateOption(option.name)) {
                report.rateControlApplied = true;
            }
        }

        for (const std::string& diagnostic : optionPlan.diagnostics) {
            report.appliedOptions.emplace_back("bitrate_adapter=" + diagnostic);
        }
    }

} // namespace

    std::string VideoEncodeOptionsApplyReport::describe() const
    {
        std::ostringstream oss;
        oss << "bitrate=" << bitRate
            << ", min_bitrate=" << minBitRate
            << ", max_bitrate=" << maxBitRate
            << ", buffer_size=" << bufferSize
            << ", gop=" << gopSize
            << ", bframes=" << maxBFrames
            << ", rc_option=" << rateControlApplied
            << ", preset=" << presetApplied
            << ", tune=" << tuneApplied
            << ", profile=" << profileApplied
            << ", level=" << levelApplied;

        if (!appliedOptions.empty()) {
            oss << ", applied_options=[";
            for (std::size_t i = 0; i < appliedOptions.size(); ++i) {
                if (i > 0) {
                    oss << ";";
                }
                oss << appliedOptions[i];
            }
            oss << "]";
        }

        if (!unsupportedOptions.empty()) {
            oss << ", unsupported_options=[";
            for (std::size_t i = 0; i < unsupportedOptions.size(); ++i) {
                if (i > 0) {
                    oss << ";";
                }
                oss << unsupportedOptions[i];
            }
            oss << "]";
        }

        if (!failedOptions.empty()) {
            oss << ", failed_options=[";
            for (std::size_t i = 0; i < failedOptions.size(); ++i) {
                if (i > 0) {
                    oss << ";";
                }
                oss << failedOptions[i];
            }
            oss << "]";
        }

        return oss.str();
    }

    VideoEncodeOptionsApplyReport VideoEncodeOptionsApplier::apply(
        AVCodecContext* encoderContext,
        const AVCodec* encoder,
        const TranscodeConfig& config,
        int outputFps)
    {
        VideoEncodeOptionsApplyReport report;
        if (!encoderContext) {
            return report;
        }

        const VideoEncodeOptions& options = config.videoEncode;

        const VideoBitratePlan bitratePlan = VideoBitrateControlPlanner::plan(
            config,
            encoderContext->width,
            encoderContext->height,
            outputFps
        );
        for (const std::string& diagnostic : bitratePlan.diagnostics) {
            report.appliedOptions.emplace_back("bitrate_plan=" + diagnostic);
        }

        const VideoBitrateOptionPlan optionPlan = VideoBitrateOptionAdapter::adapt(
            encoder,
            bitratePlan
        );
        applyBitrateOptionPlan(encoderContext, optionPlan, report);

        encoderContext->gop_size = chooseGopSize(options, outputFps);
        encoderContext->max_b_frames = chooseMaxBFrames(options);
        report.gopSize = encoderContext->gop_size;
        report.maxBFrames = encoderContext->max_b_frames;

        applyOptionalStringOptions(encoderContext, encoder, options, report);

        return report;
    }

} // namespace media::ffmpeg
