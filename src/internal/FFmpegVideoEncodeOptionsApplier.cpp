#include "internal/FFmpegVideoEncodeOptionsApplier.h"

#include <algorithm>
#include <sstream>

extern "C" {
#include <libavutil/avstring.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

namespace media::ffmpeg {
namespace {

    int64_t kbpsToBps(int kbps)
    {
        return static_cast<int64_t>(std::max(1, kbps)) * 1000;
    }

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

    const char* rateControlModeName(VideoRateControlMode mode)
    {
        switch (mode) {
        case VideoRateControlMode::CBR:
            return "cbr";
        case VideoRateControlMode::VBR:
            return "vbr";
        case VideoRateControlMode::Auto:
        default:
            return nullptr;
        }
    }

    bool applyRateControlMode(AVCodecContext* encoderContext,
                              VideoRateControlMode mode,
                              VideoEncodeOptionsApplyReport& report)
    {
        const char* modeName = rateControlModeName(mode);
        if (!modeName) {
            return false;
        }

        // FFmpeg encoders use different option names for rate control. Try the
        // common names, but only after probing option availability.
        static const char* const optionNames[] = {
            "rc",
            "rate_control",
            "rc_mode",
            nullptr
        };

        for (const char* const* p = optionNames; *p; ++p) {
            if (setStringOptionIfSupported(encoderContext, *p, modeName, report)) {
                report.rateControlApplied = true;
                return true;
            }
        }

        return false;
    }

    void applyOptionalStringOptions(AVCodecContext* encoderContext,
                                    const VideoEncodeOptions& options,
                                    VideoEncodeOptionsApplyReport& report)
    {
        report.presetApplied = setStringOptionIfSupported(
            encoderContext,
            "preset",
            options.preset,
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

    void applyCommonRateControlFields(AVCodecContext* encoderContext,
                                      const TranscodeConfig& config,
                                      VideoEncodeOptionsApplyReport& report)
    {
        const VideoEncodeOptions& options = config.videoEncode;

        encoderContext->bit_rate = kbpsToBps(config.videoBitrateKbps);
        report.bitRate = encoderContext->bit_rate;

        const int maxBitrateKbps = options.maxBitrateKbps > 0
            ? options.maxBitrateKbps
            : 0;
        if (maxBitrateKbps > 0) {
            encoderContext->rc_max_rate = kbpsToBps(maxBitrateKbps);
            report.maxBitRate = encoderContext->rc_max_rate;
        }

        const int bufferSizeKbps = options.bufferSizeKbps > 0
            ? options.bufferSizeKbps
            : 0;
        if (bufferSizeKbps > 0) {
            encoderContext->rc_buffer_size = static_cast<int>(kbpsToBps(bufferSizeKbps));
            report.bufferSize = encoderContext->rc_buffer_size;
        }
    }

} // namespace

    std::string VideoEncodeOptionsApplyReport::describe() const
    {
        std::ostringstream oss;
        oss << "bitrate=" << bitRate
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
        const AVCodec* /*encoder*/,
        const TranscodeConfig& config,
        int outputFps)
    {
        VideoEncodeOptionsApplyReport report;
        if (!encoderContext) {
            return report;
        }

        const VideoEncodeOptions& options = config.videoEncode;

        applyCommonRateControlFields(encoderContext, config, report);

        encoderContext->gop_size = chooseGopSize(options, outputFps);
        encoderContext->max_b_frames = chooseMaxBFrames(options);
        report.gopSize = encoderContext->gop_size;
        report.maxBFrames = encoderContext->max_b_frames;

        applyRateControlMode(encoderContext, options.rateControl, report);
        applyOptionalStringOptions(encoderContext, options, report);

        // Some encoders expose VBV/peak bitrate only as private options. Probe
        // and set them opportunistically while keeping AVCodecContext fields as
        // the portable source of truth.
        if (report.maxBitRate > 0) {
            setIntegerOptionIfSupported(encoderContext, "maxrate", report.maxBitRate, report);
        }

        if (report.bufferSize > 0) {
            setIntegerOptionIfSupported(encoderContext, "bufsize", report.bufferSize, report);
        }

        return report;
    }

} // namespace media::ffmpeg
