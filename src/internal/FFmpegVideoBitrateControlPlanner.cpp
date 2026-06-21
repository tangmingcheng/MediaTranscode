#include "internal/FFmpegVideoBitrateControlPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

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

    VideoRateControlMode resolveRateControl(const TranscodeConfig& config)
    {
        if (config.videoBitrate.rateControl != VideoRateControlMode::Auto) {
            return config.videoBitrate.rateControl;
        }

        if (config.videoEncode.rateControl != VideoRateControlMode::Auto) {
            return config.videoEncode.rateControl;
        }

        return config.bitratePolicy.defaultRateControl;
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

        // If the output is above all configured ladder entries, use the largest entry.
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

        if (config.videoBitrateKbps > 0) {
            userTargetApplied = true;
            return config.videoBitrateKbps;
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
                            bool userTargetApplied)
    {
        if (targetKbps <= 0 || userTargetApplied) {
            return targetKbps;
        }

        const double factor =
            codecFactor(config.bitratePolicy, config.videoCodec) *
            intentFactor(config.bitratePolicy, config.videoBitrate.intent) *
            contentFactor(config.bitratePolicy, config.videoBitrate.contentHint);

        return roundToInt(targetKbps * factor);
    }

    int resolveMaxKbps(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (config.videoBitrate.maxKbps > 0) {
            plan.userMaxApplied = true;
            return config.videoBitrate.maxKbps;
        }

        if (config.videoEncode.maxBitrateKbps > 0) {
            plan.userMaxApplied = true;
            return config.videoEncode.maxBitrateKbps;
        }

        return 0;
    }

    int resolveBufferKbits(const TranscodeConfig& config, VideoBitratePlan& plan)
    {
        if (config.videoBitrate.bufferSizeKbits > 0) {
            plan.userBufferApplied = true;
            return config.videoBitrate.bufferSizeKbits;
        }

        if (config.videoEncode.bufferSizeKbps > 0) {
            plan.userBufferApplied = true;
            return config.videoEncode.bufferSizeKbps;
        }

        return 0;
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

        if (plan.bufferSizeKbits <= 0 && plan.targetKbps > 0 && config.bitratePolicy.vbrBufferSeconds > 0.0) {
            plan.bufferSizeKbits = roundToInt(plan.targetKbps * config.bitratePolicy.vbrBufferSeconds);
            addDiagnostic(plan, "VBR buffer size derived from policy buffer seconds");
        }
    }

} // namespace

    VideoBitratePlan VideoBitrateControlPlanner::plan(const TranscodeConfig& config,
                                                      int outputWidth,
                                                      int outputHeight,
                                                      int outputFps)
    {
        (void)outputFps;

        VideoBitratePlan plan;
        plan.rateControl = resolveRateControl(config);

        bool userTargetApplied = false;
        int targetKbps = targetFromInputs(
            config,
            outputWidth,
            outputHeight,
            userTargetApplied,
            plan
        );

        targetKbps = scaleTargetByPolicy(config, targetKbps, userTargetApplied);
        targetKbps = clampTargetByPolicy(plan, config.bitratePolicy, targetKbps);

        plan.targetKbps = positiveOrZero(targetKbps);
        plan.userTargetApplied = userTargetApplied;

        plan.minKbps = positiveOrZero(config.videoBitrate.minKbps);
        plan.userMinApplied = plan.minKbps > 0;

        plan.maxKbps = positiveOrZero(resolveMaxKbps(config, plan));
        plan.bufferSizeKbits = positiveOrZero(resolveBufferKbits(config, plan));

        switch (plan.rateControl) {
        case VideoRateControlMode::CBR:
            completeCbrPlan(config, plan);
            break;

        case VideoRateControlMode::VBR:
            completeVbrPlan(config, plan);
            break;

        case VideoRateControlMode::Auto:
        default:
            break;
        }

        return plan;
    }

} // namespace media::ffmpeg
