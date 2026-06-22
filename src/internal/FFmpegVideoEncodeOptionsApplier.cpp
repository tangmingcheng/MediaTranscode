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

    std::string x26xPresetName(VideoEncodeSpeedPreset speedPreset)
    {
        switch (speedPreset) {
        case VideoEncodeSpeedPreset::Ultrafast:
            return "ultrafast";
        case VideoEncodeSpeedPreset::Superfast:
            return "superfast";
        case VideoEncodeSpeedPreset::Veryfast:
            return "veryfast";
        case VideoEncodeSpeedPreset::Faster:
            return "faster";
        case VideoEncodeSpeedPreset::Fast:
            return "fast";
        case VideoEncodeSpeedPreset::Slow:
            return "slow";
        case VideoEncodeSpeedPreset::Slower:
            return "slower";
        case VideoEncodeSpeedPreset::Veryslow:
            return "veryslow";
        case VideoEncodeSpeedPreset::Placebo:
            return "placebo";
        case VideoEncodeSpeedPreset::Medium:
        default:
            return "medium";
        }
    }

    std::string nvencPresetName(VideoEncodeSpeedPreset speedPreset)
    {
        switch (speedPreset) {
        case VideoEncodeSpeedPreset::Ultrafast:
        case VideoEncodeSpeedPreset::Superfast:
            return "p1";
        case VideoEncodeSpeedPreset::Veryfast:
            return "p2";
        case VideoEncodeSpeedPreset::Faster:
        case VideoEncodeSpeedPreset::Fast:
            return "p3";
        case VideoEncodeSpeedPreset::Medium:
            return "p4";
        case VideoEncodeSpeedPreset::Slow:
            return "p5";
        case VideoEncodeSpeedPreset::Slower:
            return "p6";
        case VideoEncodeSpeedPreset::Veryslow:
        case VideoEncodeSpeedPreset::Placebo:
        default:
            return "p7";
        }
    }

    std::string nativePresetForSpeed(const AVCodec* encoder,
                                     VideoEncodeSpeedPreset speedPreset)
    {
        if (!encoder || !encoder->name) {
            return {};
        }

        if (encoderNameContains(encoder, "_nvenc")) {
            return nvencPresetName(speedPreset);
        }

        if (encoderNameEquals(encoder, "libx264") || encoderNameEquals(encoder, "libx265")) {
            return x26xPresetName(speedPreset);
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

    void applyBitrateOptions(AVCodecContext* encoderContext,
                             const VideoBitrateOptionPlan& optionPlan,
                             VideoEncodeOptionsApplyReport& report)
    {
        if (optionPlan.bitRate > 0) {
            encoderContext->bit_rate = optionPlan.bitRate;
            report.bitRate = optionPlan.bitRate;
        }

        if (optionPlan.minBitRate > 0) {
            encoderContext->rc_min_rate = optionPlan.minBitRate;
            report.minBitRate = optionPlan.minBitRate;
        }

        if (optionPlan.maxBitRate > 0) {
            encoderContext->rc_max_rate = optionPlan.maxBitRate;
            report.maxBitRate = optionPlan.maxBitRate;
        }

        if (optionPlan.bufferSize > 0) {
            encoderContext->rc_buffer_size = optionPlan.bufferSize;
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

            if (applied && isRateControlPrivateOption(option.name)) {
                report.rateControlApplied = true;
            }
        }

        report.diagnostics.insert(
            report.diagnostics.end(),
            optionPlan.diagnostics.begin(),
            optionPlan.diagnostics.end()
        );
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

    void VideoEncodeOptionsApplier::apply(AVCodecContext* encoderContext,
                                          const AVCodec* encoder,
                                          const TranscodeConfig& config,
                                          int outputWidth,
                                          int outputHeight,
                                          int outputFps,
                                          VideoEncodeOptionsApplyReport& report)
    {
        if (!encoderContext) {
            return;
        }

        encoderContext->gop_size = chooseGopSize(config.videoEncode, outputFps);
        encoderContext->max_b_frames = chooseMaxBFrames(config.videoEncode);

        report.gopSize = encoderContext->gop_size;
        report.maxBFrames = encoderContext->max_b_frames;

        const VideoBitratePlan bitratePlan = VideoBitrateControlPlanner::plan(
            config,
            outputWidth,
            outputHeight,
            outputFps
        );
        report.bitratePlan = bitratePlan;

        const VideoBitrateOptionPlan optionPlan = VideoBitrateOptionAdapter::adapt(
            encoder,
            bitratePlan
        );

        applyBitrateOptions(encoderContext, optionPlan, report);
        applyOptionalStringOptions(encoderContext, encoder, config.videoEncode, report);
    }

    std::string VideoEncodeOptionsApplier::describe(const VideoEncodeOptionsApplyReport& report)
    {
        std::ostringstream oss;
        oss << "gop=" << report.gopSize
            << ", bframes=" << report.maxBFrames
            << ", bitrate=" << report.bitRate
            << ", min_bitrate=" << report.minBitRate
            << ", max_bitrate=" << report.maxBitRate
            << ", buffer=" << report.bufferSize
            << ", preset=" << (report.presetApplied ? "applied" : "default")
            << ", tune=" << (report.tuneApplied ? "applied" : "default")
            << ", profile=" << (report.profileApplied ? "applied" : "default")
            << ", level=" << (report.levelApplied ? "applied" : "default")
            << ", rc_option=" << (report.rateControlApplied ? "applied" : "default")
            << ", unsupported=[" << joinList(report.unsupportedOptions) << "]"
            << ", failed=[" << joinList(report.failedOptions) << "]"
            << ", diagnostics=[" << joinList(report.diagnostics) << "]";

        return oss.str();
    }

} // namespace media::ffmpeg
