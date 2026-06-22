#include "internal/FFmpegVideoBitrateControlPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace media::ffmpeg {
namespace {

    int positiveOrZero(int value)
    {
        return value > 0 ? value : 0;
    }

    int roundToInt(double value)
    {
        if (value <= 0.0) {
            return 0;
        }

        if (value >= static_cast<double>(std::numeric_limits<int>::max())) {
            return std::numeric_limits<int>::max();
        }

        return static_cast<int>(std::llround(value));
    }

    void addDiagnostic(VideoBitratePlan& plan, const std::string& text)
    {
        if (!text.empty()) {
            plan.diagnostics.emplace_back(text);
        }
    }

    const char* rateControlName(VideoRateControlMode mode)
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

    bool supportsQuality(VideoRateControlMode mode)
    {
        return mode == VideoRateControlMode::CRF || mode == VideoRateControlMode::CappedVBR;
    }

    bool hasUserBitrateConstraint(const TranscodeConfig& config)
    {
        return config.videoBitrate.targetKbps > 0 ||
               config.videoBitrate.minKbps > 0 ||
               config.videoBitrate.maxKbps > 0 ||
               config.videoBitrate.bufferSizeKbits > 0;
    }

    VideoRateControlMode initialRateControl(const TranscodeConfig& config)
    {
        if (config.videoBitrate.rateControl != VideoRateControlMode::Auto) {
            return config.videoBitrate.rateControl;
        }

        if (config.bitratePolicy.defaultRateControl != VideoRateControlMode::Auto) {
            return config.bitratePolicy.defaultRateControl;
        }

        return VideoRateControlMode::Auto;
    }

    VideoRateControlMode autoSelectRateControl(const TranscodeConfig& config,
                                               const VideoBitratePlan& plan)
    {
        if (plan.rateControl != VideoRateControlMode::Auto) {
            return plan.rateControl;
        }

        if (config.videoBitrate.quality > 0) {
            return hasUserBitrateConstraint(config)
                ? VideoRateControlMode::CappedVBR
                : VideoRateControlMode::CRF;
        }

        if (plan.targetKbps > 0) {
            if (plan.maxKbps > 0 && plan.maxKbps <= plan.targetKbps) {
                return VideoRateControlMode::CBR;
            }

            return VideoRateControlMode::VBR;
        }

        return VideoRateControlMode::Auto;
    }

    int chooseTargetFromLadder(const VideoBitrateControlPolicy& policy,
                               int outputWidth,
                               int outputHeight)
    {
        if (outputWidth <= 0 || outputHeight <= 0 || policy.ladderRules.empty()) {
            return 0;
        }

        const int64_t pixels = static_cast<int64_t>(outputWidth) * outputHeight;
        int selectedPixels = std::numeric_limits<int>::max();
        int selectedKbps = 0;

        for (const VideoBitrateLadderRule& rule : policy.ladderRules) {
            if (rule.maxPixels <= 0 || rule.targetKbps <= 0) {
                continue;
            }

            if (pixels <= rule.maxPixels && rule.maxPixels < selectedPixels) {
                selectedPixels = rule.maxPixels;
                selectedKbps = rule.targetKbps;
            }
        }

        if (selectedKbps > 0) {
            return selectedKbps;
        }

        int largestPixels = 0;
        for (const VideoBitrateLadderRule& rule : policy.ladderRules) {
            if (rule.maxPixels > largestPixels && rule.targetKbps > 0) {
                largestPixels = rule.maxPixels;
                selectedKbps = rule.targetKbps;
            }
        }

        return selectedKbps;
    }

    double codecFactor(const VideoBitrateControlPolicy& policy, VideoCodec codec)
    {
        for (const VideoCodecBitrateFactor& item : policy.codecFactors) {
            if (item.codec == codec && item.factor > 0.0) {
                return item.factor;
            }
        }

        return 1.0;
    }

    double intentFactor(const VideoBitrateControlPolicy& policy, VideoBitrateIntent intent)
    {
        if (intent == VideoBitrateIntent::Auto) {
            return 1.0;
        }

        for (const VideoBitrateIntentFactor& item : policy.intentFactors) {
            if (item.intent == intent && item.factor > 0.0) {
                return item.factor;
            }
        }

        return 1.0;
    }

    double contentFactor(const VideoBitrateControlPolicy& policy, VideoContentHint content)
    {
        if (content == VideoContentHint::Auto) {
            return 1.0;
        }

        for (const VideoContentBitrateFactor& item : policy.contentFactors) {
            if (item.content == content && item.factor > 0.0) {
                return item.factor;
            }
        }

        return 1.0;
    }

    double fpsFactor(const VideoBitrateControlPolicy& policy, int outputFps)
    {
        if (policy.referenceFps <= 0.0 || outputFps <= 0) {
            return 1.0;
        }

        double factor = static_cast<double>(outputFps) / policy.referenceFps;

        if (policy.minimumFpsFactor > 0.0) {
            factor = std::max(factor, policy.minimumFpsFactor);
        }

        if (policy.maximumFpsFactor > 0.0) {
            factor = std::min(factor, policy.maximumFpsFactor);
        }

        return factor;
    }

    int clampTargetByPolicy(VideoBitratePlan& plan,
                            const VideoBitrateControlPolicy& policy,
                            int targetKbps)
    {
        int result = targetKbps;

        if (policy.minimumTargetKbps > 0 && result > 0 && result < policy.minimumTargetKbps) {
            std::ostringstream oss;
            oss << "target bitrate raised by policy minimum: "
                << result << " -> " << policy.minimumTargetKbps << " kbps";
            addDiagnostic(plan, oss.str());
            result = policy.minimumTargetKbps;
        }

        if (policy.maximumTargetKbps > 0 && result > policy.maximumTargetKbps) {
            std::ostringstream oss;
            oss << "target bitrate limited by policy maximum: "
                << result << " -> " << policy.maximumTargetKbps << " kbps";
            addDiagnostic(plan, oss.str());
            result = policy.maximumTargetKbps;
        }

        return result;
    }

    int clampQualityByPolicy(VideoBitratePlan& plan,
                             const VideoBitrateControlPolicy& policy,
                             int quality)
    {
        int result = quality;

        if (result <= 0 && policy.defaultQuality > 0) {
            result = policy.defaultQuality;
            addDiagnostic(plan, "quality selected from policy default");
        }

        if (policy.minimumQuality > 0 && result > 0 && result < policy.minimumQuality) {
            std::ostringstream oss;
            oss << "quality raised by policy minimum: "
                << result << " -> " << policy.minimumQuality;
            addDiagnostic(plan, oss.str());
            result = policy.minimumQuality;
        }

        if (policy.maximumQuality > 0 && result > policy.maximumQuality) {
            std::ostringstream oss;
            oss << "quality limited by policy maximum: "
                << result << " -> " << policy.maximumQuality;
            addDiagnostic(plan, oss.str());
            result = policy.maximumQuality;
        }

        return result;
    }

    int targetFromInputs(const TranscodeConfig& config,
                         int outputWidth,
                         int outputHeight,
                         bool& userTargetApplied,
                         VideoBitratePlan& plan)
    {
        userTargetApplied = false;

        if (config.videoBitrate.targetKbps > 0) {
            userTargetApplied = true;
            return config.videoBitrate.targetKbps;
        }

        int targetKbps = chooseTargetFromLadder(
            config.bitratePolicy,
            outputWidth,
            outputHeight
        );
        if (targetKbps > 0) {
            addDiagnostic(plan, "target bitrate selected from policy ladder");
            return targetKbps;
        }

        if (config.bitratePolicy.fallbackTargetKbps > 0) {
            addDiagnostic(plan, "target bitrate selected from policy fallback");
            return config.bitratePolicy.fallbackTargetKbps;
        }

        return 0;
    }

    int scaleTargetByPolicy(const TranscodeConfig& config,
                            int targetKbps,
                            bool userTargetApplied,
                            int outputFps,
                            VideoBitratePlan& plan)
    {
        if (targetKbps <= 0 || userTargetApplied) {
            return targetKbps;
        }

        const double factor =
            codecFactor(config.bitratePolicy, config.videoCodec) *
            intentFactor(config.bitratePolicy, config.videoBitrate.intent) *
            contentFactor(config.bitratePolicy, config.videoBitrate.contentHint) *
            fpsFactor(config.bitratePolicy, outputFps);

        const int scaled = roundToInt(targetKbps * factor);
        if (scaled > 0 && scaled != targetKbps) {
            std::ostringstream oss;
            oss << "target bitrate scaled by policy factors: "
                << targetKbps << " -> " << scaled << " kbps";
            addDiagnostic(plan, oss.str());
        }

        return scaled;
    }

    int resolveMaxKbps(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (config.videoBitrate.maxKbps > 0) {
            plan.userMaxApplied = true;
            return config.videoBitrate.maxKbps;
        }

        return 0;
    }

    int resolveBufferKbits(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (config.videoBitrate.bufferSizeKbits > 0) {
            plan.userBufferApplied = true;
            return config.videoBitrate.bufferSizeKbits;
        }

        return 0;
    }

    void resolveQuality(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (!supportsQuality(plan.rateControl)) {
            if (config.videoBitrate.quality > 0) {
                std::ostringstream oss;
                oss << "quality ignored by rate control mode: " << rateControlName(plan.rateControl);
                addDiagnostic(plan, oss.str());
            }
            plan.quality = 0;
            plan.userQualityApplied = false;
            return;
        }

        if (config.videoBitrate.quality > 0) {
            plan.quality = config.videoBitrate.quality;
            plan.userQualityApplied = true;
        }

        plan.quality = positiveOrZero(clampQualityByPolicy(plan, config.bitratePolicy, plan.quality));
    }

    void enforceOrderedConstraints(VideoBitratePlan& plan)
    {
        if (plan.maxKbps > 0 && plan.targetKbps > plan.maxKbps) {
            std::ostringstream oss;
            oss << "target bitrate limited by max constraint: "
                << plan.targetKbps << " -> " << plan.maxKbps << " kbps";
            addDiagnostic(plan, oss.str());
            plan.targetKbps = plan.maxKbps;
        }

        if (plan.targetKbps > 0 && plan.minKbps > plan.targetKbps) {
            std::ostringstream oss;
            oss << "min bitrate limited by target constraint: "
                << plan.minKbps << " -> " << plan.targetKbps << " kbps";
            addDiagnostic(plan, oss.str());
            plan.minKbps = plan.targetKbps;
        }

        if (plan.maxKbps > 0 && plan.minKbps > plan.maxKbps) {
            std::ostringstream oss;
            oss << "min bitrate limited by max constraint: "
                << plan.minKbps << " -> " << plan.maxKbps << " kbps";
            addDiagnostic(plan, oss.str());
            plan.minKbps = plan.maxKbps;
        }
    }

    void deriveMinFromRatio(VideoBitratePlan& plan, double ratio, const char* label)
    {
        if (plan.minKbps > 0 || plan.targetKbps <= 0 || ratio <= 0.0) {
            return;
        }

        plan.minKbps = roundToInt(plan.targetKbps * ratio);
        if (plan.minKbps > 0) {
            std::ostringstream oss;
            oss << label << " min bitrate derived from target ratio: " << plan.minKbps << " kbps";
            addDiagnostic(plan, oss.str());
        }
    }

    void completeCbrPlan(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (plan.maxKbps <= 0 && plan.targetKbps > 0) {
            if (config.bitratePolicy.cbrPeakMultiplier > 0.0) {
                plan.maxKbps = roundToInt(plan.targetKbps * config.bitratePolicy.cbrPeakMultiplier);
                addDiagnostic(plan, "CBR max bitrate derived from policy peak multiplier");
            }
            else {
                plan.maxKbps = plan.targetKbps;
                addDiagnostic(plan, "CBR max bitrate matched to target bitrate");
            }
        }

        if (plan.minKbps <= 0 && plan.targetKbps > 0) {
            if (config.bitratePolicy.cbrMinToTargetRatio > 0.0) {
                deriveMinFromRatio(plan, config.bitratePolicy.cbrMinToTargetRatio, "CBR");
            }
            else {
                plan.minKbps = plan.targetKbps;
                addDiagnostic(plan, "CBR min bitrate matched to target bitrate");
            }
        }

        if (plan.bufferSizeKbits <= 0 && plan.targetKbps > 0 && config.bitratePolicy.cbrBufferSeconds > 0.0) {
            plan.bufferSizeKbits = roundToInt(plan.targetKbps * config.bitratePolicy.cbrBufferSeconds);
            addDiagnostic(plan, "CBR buffer size derived from policy buffer seconds");
        }
    }

    void completeVbrPlan(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (plan.maxKbps <= 0 && plan.targetKbps > 0 && config.bitratePolicy.vbrPeakMultiplier > 0.0) {
            plan.maxKbps = roundToInt(plan.targetKbps * config.bitratePolicy.vbrPeakMultiplier);
            addDiagnostic(plan, "VBR max bitrate derived from policy peak multiplier");
        }

        deriveMinFromRatio(plan, config.bitratePolicy.vbrMinToTargetRatio, "VBR");

        if (plan.bufferSizeKbits <= 0 && plan.targetKbps > 0 && config.bitratePolicy.vbrBufferSeconds > 0.0) {
            plan.bufferSizeKbits = roundToInt(plan.targetKbps * config.bitratePolicy.vbrBufferSeconds);
            addDiagnostic(plan, "VBR buffer size derived from policy buffer seconds");
        }
    }

    void completeCrfPlan(VideoBitratePlan& plan)
    {
        if (plan.quality <= 0) {
            plan.quality = 23;
            addDiagnostic(plan, "CRF quality defaulted to 23");
        }

        if (plan.userTargetApplied || plan.userMinApplied || plan.userMaxApplied || plan.userBufferApplied) {
            addDiagnostic(plan, "bitrate constraints ignored by CRF mode");
        }

        plan.targetKbps = 0;
        plan.minKbps = 0;
        plan.maxKbps = 0;
        plan.bufferSizeKbits = 0;
    }

    void completeCappedVbrPlan(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (plan.quality <= 0) {
            plan.quality = 23;
            addDiagnostic(plan, "capped VBR quality defaulted to 23");
        }

        completeVbrPlan(config, plan);

        if (plan.maxKbps <= 0 && plan.targetKbps > 0) {
            plan.maxKbps = plan.targetKbps;
            addDiagnostic(plan, "capped VBR max bitrate defaulted to target bitrate");
        }
    }

    void completeByRateControl(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        switch (plan.rateControl) {
        case VideoRateControlMode::CBR:
            completeCbrPlan(config, plan);
            break;

        case VideoRateControlMode::VBR:
            completeVbrPlan(config, plan);
            break;

        case VideoRateControlMode::CRF:
            completeCrfPlan(plan);
            break;

        case VideoRateControlMode::CappedVBR:
            completeCappedVbrPlan(config, plan);
            break;

        case VideoRateControlMode::Auto:
        default:
            break;
        }
    }

} // namespace

    VideoBitratePlan VideoBitrateControlPlanner::plan(const TranscodeConfig& config,
                                                      int outputWidth,
                                                      int outputHeight,
                                                      int outputFps)
    {
        VideoBitratePlan plan;
        plan.rateControl = initialRateControl(config);

        bool userTargetApplied = false;
        int targetKbps = targetFromInputs(
            config,
            outputWidth,
            outputHeight,
            userTargetApplied,
            plan
        );

        targetKbps = scaleTargetByPolicy(config, targetKbps, userTargetApplied, outputFps, plan);
        targetKbps = clampTargetByPolicy(plan, config.bitratePolicy, targetKbps);

        plan.targetKbps = positiveOrZero(targetKbps);
        plan.userTargetApplied = userTargetApplied;

        plan.minKbps = positiveOrZero(config.videoBitrate.minKbps);
        plan.userMinApplied = plan.minKbps > 0;

        plan.maxKbps = positiveOrZero(resolveMaxKbps(config, plan));
        plan.bufferSizeKbits = positiveOrZero(resolveBufferKbits(config, plan));

        plan.rateControl = autoSelectRateControl(config, plan);
        {
            std::ostringstream oss;
            oss << "policy engine selected rc=" << rateControlName(plan.rateControl);
            addDiagnostic(plan, oss.str());
        }

        resolveQuality(config, plan);
        completeByRateControl(config, plan);
        enforceOrderedConstraints(plan);

        return plan;
    }

} // namespace media::ffmpeg
