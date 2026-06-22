#include "internal/FFmpegVideoBitrateOptionAdapter.h"

#include "internal/FFmpegEncoderCapabilityMatrix.h"

#include <string>
#include <vector>

namespace media::ffmpeg {
namespace {

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

    bool isQualityDrivenMode(VideoRateControlMode mode)
    {
        return mode == VideoRateControlMode::CRF || mode == VideoRateControlMode::CappedVBR;
    }

    void addDiagnostic(VideoBitrateOptionPlan& output, const std::string& text)
    {
        if (!text.empty()) {
            output.diagnostics.emplace_back(text);
        }
    }

    void applyCommonBitrateFields(const VideoBitratePlan& plan,
                                  const FFmpegEncoderCapabilities& capabilities,
                                  VideoBitrateOptionPlan& output)
    {
        if (plan.targetKbps > 0 && capabilities.supportsBitRateField) {
            output.bitRate = kbpsToBps(plan.targetKbps);
        }

        if (plan.minKbps > 0 && capabilities.supportsMinRateField) {
            output.minBitRate = kbpsToBps(plan.minKbps);
        }

        if (plan.maxKbps > 0 && capabilities.supportsMaxRateField) {
            output.maxBitRate = kbpsToBps(plan.maxKbps);
        }

        if (plan.bufferSizeKbits > 0 && capabilities.supportsBufferSizeField) {
            output.bufferSize = kbpsToBps(plan.bufferSizeKbits);
        }
    }

    void clearBitrateFields(VideoBitrateOptionPlan& output)
    {
        output.bitRate = 0;
        output.minBitRate = 0;
        output.maxBitRate = 0;
        output.bufferSize = 0;
    }

    void addPrivateVbvOptions(const FFmpegEncoderCapabilities& capabilities,
                              const VideoBitrateOptionPlan& output,
                              std::vector<VideoBitrateOption>& options)
    {
        if (!capabilities.supportsPrivateVbvOptions) {
            return;
        }

        if (output.minBitRate > 0) {
            options.emplace_back(integerOption("minrate", output.minBitRate));
        }

        if (output.maxBitRate > 0) {
            options.emplace_back(integerOption("maxrate", output.maxBitRate));
        }

        if (output.bufferSize > 0) {
            options.emplace_back(integerOption("bufsize", output.bufferSize));
        }
    }

    void addRateControlOption(const FFmpegEncoderCapabilities& capabilities,
                              const VideoBitratePlan& plan,
                              VideoBitrateOptionPlan& output)
    {
        if (plan.rateControl == VideoRateControlMode::Auto) {
            return;
        }

        if (!FFmpegEncoderCapabilityMatrix::supportsRateControl(capabilities, plan.rateControl)) {
            addDiagnostic(
                output,
                "rate control unsupported by encoder family: " +
                    capabilities.familyName + "/" +
                    FFmpegEncoderCapabilityMatrix::rateControlName(plan.rateControl)
            );
            return;
        }

        if (plan.rateControl == VideoRateControlMode::CBR && capabilities.supportsNalHrdCbr) {
            output.privateOptions.emplace_back(stringOption("nal-hrd", "cbr"));
            return;
        }

        if (capabilities.rateControlOptionName.empty()) {
            return;
        }

        const std::string value = FFmpegEncoderCapabilityMatrix::rateControlValue(
            capabilities,
            plan.rateControl
        );
        if (!value.empty()) {
            output.privateOptions.emplace_back(stringOption(capabilities.rateControlOptionName, value));
        }
    }

    void addQualityOption(const FFmpegEncoderCapabilities& capabilities,
                          const VideoBitratePlan& plan,
                          VideoBitrateOptionPlan& output)
    {
        if (!isQualityDrivenMode(plan.rateControl) || plan.quality <= 0) {
            return;
        }

        if (capabilities.qualityOptionName.empty()) {
            addDiagnostic(output, "quality option unavailable for encoder family: " + capabilities.familyName);
            return;
        }

        if (capabilities.qualityOptionInteger) {
            output.privateOptions.emplace_back(integerOption(capabilities.qualityOptionName, plan.quality));
        }
        else {
            output.privateOptions.emplace_back(stringOption(capabilities.qualityOptionName, std::to_string(plan.quality)));
        }
    }

} // namespace

    VideoBitrateOptionPlan VideoBitrateOptionAdapter::adapt(const AVCodec* encoder,
                                                            const VideoBitratePlan& plan)
    {
        VideoBitrateOptionPlan output;
        const FFmpegEncoderCapabilities capabilities = FFmpegEncoderCapabilityMatrix::query(encoder);

        applyCommonBitrateFields(plan, capabilities, output);

        addRateControlOption(capabilities, plan, output);
        addQualityOption(capabilities, plan, output);

        if (plan.rateControl == VideoRateControlMode::CRF) {
            clearBitrateFields(output);
        }

        addPrivateVbvOptions(capabilities, output, output.privateOptions);

        output.diagnostics.emplace_back("adapter=" + capabilities.familyName);
        output.diagnostics.emplace_back("encoder=" + capabilities.encoderName);
        output.diagnostics.emplace_back("rc=" + FFmpegEncoderCapabilityMatrix::rateControlName(plan.rateControl));
        if (plan.quality > 0 && isQualityDrivenMode(plan.rateControl)) {
            output.diagnostics.emplace_back("quality=" + std::to_string(plan.quality));
        }
        if (output.minBitRate > 0) {
            output.diagnostics.emplace_back("minrate=" + std::to_string(output.minBitRate));
        }

        return output;
    }

} // namespace media::ffmpeg
