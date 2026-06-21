#include "internal/FFmpegVideoBitrateOptionAdapter.h"

#include <string>

namespace media::ffmpeg {
namespace {

    bool encoderNameContains(const AVCodec* encoder, const char* token)
    {
        return encoder && encoder->name && std::string(encoder->name).find(token) != std::string::npos;
    }

    bool encoderNameEquals(const AVCodec* encoder, const char* name)
    {
        return encoder && encoder->name && std::string(encoder->name) == name;
    }

    VideoBitrateOption stringOption(const std::string& name, const std::string& value)
    {
        VideoBitrateOption option;
        option.type = VideoBitrateOption::Type::String;
        option.name = name;
        option.stringValue = value;
        return option;
    }

    VideoBitrateOption integerOption(const std::string& name, int64_t value)
    {
        VideoBitrateOption option;
        option.type = VideoBitrateOption::Type::Integer;
        option.name = name;
        option.integerValue = value;
        return option;
    }

    int64_t kbpsToBps(int kbps)
    {
        return static_cast<int64_t>(kbps) * 1000;
    }

    bool isX26xEncoder(const AVCodec* encoder)
    {
        return encoderNameEquals(encoder, "libx264") || encoderNameEquals(encoder, "libx265");
    }

    bool isNvencEncoder(const AVCodec* encoder)
    {
        return encoderNameContains(encoder, "_nvenc");
    }

    bool isQualityDrivenMode(VideoRateControlMode mode)
    {
        return mode == VideoRateControlMode::CRF || mode == VideoRateControlMode::CappedVBR;
    }

    void applyCommonBitrateFields(const VideoBitratePlan& plan, VideoBitrateOptionPlan& output)
    {
        if (plan.targetKbps > 0) {
            output.bitRate = kbpsToBps(plan.targetKbps);
        }

        if (plan.maxKbps > 0) {
            output.maxBitRate = kbpsToBps(plan.maxKbps);
        }

        if (plan.bufferSizeKbits > 0) {
            output.bufferSize = kbpsToBps(plan.bufferSizeKbits);
        }
    }

    void adaptX26x(const VideoBitratePlan& plan, VideoBitrateOptionPlan& output)
    {
        switch (plan.rateControl) {
        case VideoRateControlMode::CBR:
            output.privateOptions.emplace_back(stringOption("nal-hrd", "cbr"));
            break;

        case VideoRateControlMode::VBR:
            break;

        case VideoRateControlMode::CRF:
            if (plan.quality > 0) {
                output.privateOptions.emplace_back(integerOption("crf", plan.quality));
            }
            output.bitRate = 0;
            output.maxBitRate = 0;
            output.bufferSize = 0;
            break;

        case VideoRateControlMode::CappedVBR:
            if (plan.quality > 0) {
                output.privateOptions.emplace_back(integerOption("crf", plan.quality));
            }
            break;

        case VideoRateControlMode::Auto:
        default:
            break;
        }
    }

    void adaptNvenc(const VideoBitratePlan& plan, VideoBitrateOptionPlan& output)
    {
        switch (plan.rateControl) {
        case VideoRateControlMode::CBR:
            output.privateOptions.emplace_back(stringOption("rc", "cbr"));
            break;

        case VideoRateControlMode::VBR:
            output.privateOptions.emplace_back(stringOption("rc", "vbr"));
            break;

        case VideoRateControlMode::CRF:
            output.privateOptions.emplace_back(stringOption("rc", "vbr"));
            if (plan.quality > 0) {
                output.privateOptions.emplace_back(integerOption("cq", plan.quality));
            }
            output.bitRate = 0;
            output.maxBitRate = 0;
            output.bufferSize = 0;
            break;

        case VideoRateControlMode::CappedVBR:
            output.privateOptions.emplace_back(stringOption("rc", "vbr"));
            if (plan.quality > 0) {
                output.privateOptions.emplace_back(integerOption("cq", plan.quality));
            }
            break;

        case VideoRateControlMode::Auto:
        default:
            break;
        }
    }

    void adaptGeneric(const VideoBitratePlan& plan, VideoBitrateOptionPlan& output)
    {
        switch (plan.rateControl) {
        case VideoRateControlMode::CBR:
            output.privateOptions.emplace_back(stringOption("rc", "cbr"));
            break;

        case VideoRateControlMode::VBR:
        case VideoRateControlMode::CappedVBR:
            output.privateOptions.emplace_back(stringOption("rc", "vbr"));
            break;

        case VideoRateControlMode::CRF:
            if (plan.quality > 0) {
                output.privateOptions.emplace_back(integerOption("crf", plan.quality));
            }
            output.bitRate = 0;
            output.maxBitRate = 0;
            output.bufferSize = 0;
            break;

        case VideoRateControlMode::Auto:
        default:
            break;
        }
    }

    const char* rateControlModeName(VideoRateControlMode mode)
    {
        switch (mode) {
        case VideoRateControlMode::CBR:
            return "cbr";
        case VideoRateControlMode::VBR:
            return "vbr";
        case VideoRateControlMode::CRF:
            return "crf";
        case VideoRateControlMode::CappedVBR:
            return "capped-vbr";
        case VideoRateControlMode::Auto:
        default:
            return "auto";
        }
    }

} // namespace

    VideoBitrateOptionPlan VideoBitrateOptionAdapter::adapt(const AVCodec* encoder,
                                                            const VideoBitratePlan& plan)
    {
        VideoBitrateOptionPlan output;
        applyCommonBitrateFields(plan, output);

        if (isX26xEncoder(encoder)) {
            adaptX26x(plan, output);
            output.diagnostics.emplace_back("adapter=x26x");
        }
        else if (isNvencEncoder(encoder)) {
            adaptNvenc(plan, output);
            output.diagnostics.emplace_back("adapter=nvenc");
        }
        else {
            adaptGeneric(plan, output);
            output.diagnostics.emplace_back("adapter=generic");
        }

        output.diagnostics.emplace_back(std::string("rc=") + rateControlModeName(plan.rateControl));
        if (plan.quality > 0 && isQualityDrivenMode(plan.rateControl)) {
            output.diagnostics.emplace_back("quality=" + std::to_string(plan.quality));
        }

        return output;
    }

} // namespace media::ffmpeg
