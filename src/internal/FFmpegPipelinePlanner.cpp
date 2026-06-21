#include "internal/FFmpegPipelinePlanner.h"

#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegVideoEncoderSelector.h"

#include "spdlog/spdlog.h"

#include <optional>
#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    constexpr int kScoreHardwareDecodeFilterEncode = 3000;
    constexpr int kScoreHardwareDecodeDirectEncode = 2800;
    constexpr int kScoreHardwareDecodeHardwareEncode = 2000;
    constexpr int kScoreHardwareDecodeOnly = 1000;

    std::string hardwareDeviceName(HardwareDeviceType type)
    {
        const HardwareBackendProfile profile = HardwareBackendRegistry::profileFor(type);
        if (profile.name && *profile.name) {
            return profile.name;
        }

        const char* name = HardwareDeviceContext::toAVDeviceName(type);
        return name && *name ? name : "none";
    }

    std::string avHardwareDeviceName(AVHWDeviceType type)
    {
        const char* name = av_hwdevice_get_type_name(type);
        if (name && *name) {
            return name;
        }

        return "none";
    }

    std::string pixelFormatName(AVPixelFormat format)
    {
        const char* name = av_get_pix_fmt_name(format);
        return name ? name : "none";
    }

    std::string executionModeName(VideoExecutionMode mode)
    {
        switch (mode) {
        case VideoExecutionMode::ZeroCopy:
            return "zero-copy";
        case VideoExecutionMode::MixedGpu:
            return "mixed-gpu";
        case VideoExecutionMode::Cpu:
        default:
            return "cpu";
        }
    }

    std::string videoCodecName(VideoCodec codec)
    {
        switch (codec) {
        case VideoCodec::H264:
            return "h264";
        case VideoCodec::H265:
            return "h265";
        case VideoCodec::MPEG4:
            return "mpeg4";
        case VideoCodec::VP8:
            return "vp8";
        case VideoCodec::VP9:
            return "vp9";
        case VideoCodec::AV1:
            return "av1";
        case VideoCodec::Copy:
        default:
            return "copy";
        }
    }

    bool zeroCopyWouldRequireSpatialFilter(const TranscodeConfig& config)
    {
        return config.width > 0 || config.height > 0;
    }

    std::string zeroCopyFilterBlockReason(const HardwareBackendProfile& backend,
                                          const TranscodeConfig& config)
    {
        if (!zeroCopyWouldRequireSpatialFilter(config)) {
            return {};
        }

        if (!backend.supportsZeroCopyFilter) {
            return "zero-copy backend does not provide an on-device spatial filter";
        }

        if (!backend.scaleFilterName || !*backend.scaleFilterName) {
            return "zero-copy backend does not provide a scale filter name";
        }

        return {};
    }

    bool betterAttempt(const HardwarePipelinePlanAttempt& lhs,
                       const HardwarePipelinePlanAttempt& rhs)
    {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }

        return static_cast<int>(lhs.requestedDeviceType) < static_cast<int>(rhs.requestedDeviceType);
    }

    HardwareEncoderSelection makeGenericEncoderSelection(
        const VideoEncoderSelection& genericSelection,
        const HardwareBackendProfile& backend)
    {
        HardwareEncoderSelection selection;
        selection.encoder = genericSelection.encoder;
        selection.encoderName = genericSelection.encoderName;
        selection.pixelFormat = genericSelection.pixelFormat;
        selection.zeroCopy = false;
        selection.hardwareEncoder = selection.encoder
            ? isHardwareEncoderName(selection.encoder->name)
            : false;
        selection.backend = backend;
        selection.diagnostic = genericSelection.diagnostic;
        return selection;
    }

    void applySelectedAttemptToPlan(const HardwarePipelinePlanAttempt& attempt,
                                    HardwarePipelinePlan& plan)
    {
        plan.valid = true;
        plan.zeroCopy = attempt.executionMode == VideoExecutionMode::ZeroCopy;
        plan.executionMode = attempt.executionMode;
        plan.backend = attempt.backend;
        plan.decoderConfig = attempt.decoderConfig;
        plan.encoderSelection = attempt.encoderSelection;

        std::ostringstream oss;
        oss << "selected " << executionModeName(attempt.executionMode)
            << " backend=" << hardwareDeviceName(plan.backend.deviceType)
            << ", score=" << attempt.score
            << ", decoder=" << plan.decoderConfig.decoderName
            << ", decoder_hw_pix_fmt=" << pixelFormatName(plan.decoderConfig.hardwarePixelFormat)
            << ", encoder=" << plan.encoderSelection.encoderName
            << ", encoder_input_pix_fmt=" << pixelFormatName(plan.encoderSelection.pixelFormat)
            << ", reason=" << attempt.reason;
        plan.diagnostic = oss.str();
    }

    void logAttempt(const HardwarePipelinePlanAttempt& attempt)
    {
        spdlog::info(
            "[PLAN] backend={} mode={} score={} decoderAccepted={} encoderAccepted={} reason={}",
            hardwareDeviceName(attempt.requestedDeviceType),
            executionModeName(attempt.executionMode),
            attempt.score,
            attempt.decoderAccepted,
            attempt.encoderAccepted,
            attempt.reason
        );

        if (!attempt.decoderAccepted) {
            return;
        }

        spdlog::info(
            "[PLAN]   decoder: name={}, hw_pix_fmt={}, av_device_type={}",
            attempt.decoderConfig.decoderName.empty() ? "unknown" : attempt.decoderConfig.decoderName,
            pixelFormatName(attempt.decoderConfig.hardwarePixelFormat),
            avHardwareDeviceName(attempt.decoderConfig.avDeviceType)
        );

        for (const HardwareEncoderCandidate& candidate : attempt.encoderSelection.candidates) {
            spdlog::info(
                "[PLAN]   encoder candidate={} available={} hardwareEncoder={} supportsHwFrames={} pix_fmts={} reason={}",
                candidate.encoderName,
                candidate.available,
                candidate.hardwareEncoder,
                candidate.supportsBackendHardwareFrames,
                candidate.pixelFormats,
                candidate.rejectionReason
            );
        }
    }

} // namespace

    std::vector<HardwareDeviceType> FFmpegPipelinePlanner::backendPriority(
        HardwareDeviceType requestedDeviceType)
    {
        return HardwareBackendRegistry::backendPriority(requestedDeviceType);
    }

    HardwarePipelinePlan FFmpegPipelinePlanner::planHardwarePipeline(
        const TranscodeConfig& config,
        const AVCodec* decoder)
    {
        HardwarePipelinePlan plan;
        plan.allowFallback = config.hardware.allowZeroCopyFallback;

        spdlog::info(
            "[PLAN] request: videoCodec={}, hardwareEnabled={}, preferLowestCpu=true, strictZeroCopy={}, requestedDevice={}",
            videoCodecName(config.videoCodec),
            config.hardware.enabled,
            !plan.allowFallback,
            hardwareDeviceName(HardwareDeviceType::Auto)
        );

        if (!config.hardware.enabled) {
            plan.valid = false;
            plan.executionMode = VideoExecutionMode::Cpu;
            plan.diagnostic = "hardware pipeline disabled by config";
            spdlog::warn("[PLAN] {}", plan.diagnostic);
            return plan;
        }

        if (!decoder) {
            plan.valid = false;
            plan.diagnostic = "hardware pipeline planning failed: decoder is null";
            spdlog::error("[PLAN] {}", plan.diagnostic);
            return plan;
        }

        const std::vector<HardwareDeviceType> priority = backendPriority(HardwareDeviceType::Auto);
        std::optional<HardwarePipelinePlanAttempt> bestAttempt;

        for (HardwareDeviceType deviceType : priority) {
            HardwarePipelinePlanAttempt baseAttempt;
            baseAttempt.requestedDeviceType = deviceType;
            baseAttempt.decoderConfig = HardwareDecoderSupport::findConfig(decoder, deviceType);

            if (!baseAttempt.decoderConfig.valid) {
                baseAttempt.decoderAccepted = false;
                baseAttempt.reason = baseAttempt.decoderConfig.unavailableReason.empty()
                    ? "decoder does not support backend hardware frames"
                    : baseAttempt.decoderConfig.unavailableReason;
                plan.attempts.emplace_back(baseAttempt);
                logAttempt(plan.attempts.back());
                continue;
            }

            baseAttempt.decoderAccepted = true;
            baseAttempt.backend = HardwareBackendRegistry::profileFor(baseAttempt.decoderConfig.deviceType);

            if (baseAttempt.backend.deviceType == HardwareDeviceType::None ||
                baseAttempt.backend.hardwarePixelFormat == AV_PIX_FMT_NONE) {
                baseAttempt.reason = "backend profile is not available";
                plan.attempts.emplace_back(baseAttempt);
                logAttempt(plan.attempts.back());
                continue;
            }

            const std::string filterBlockReason = zeroCopyFilterBlockReason(
                baseAttempt.backend,
                config
            );

            if (filterBlockReason.empty()) {
                HardwarePipelinePlanAttempt zeroCopyAttempt = baseAttempt;
                zeroCopyAttempt.executionMode = VideoExecutionMode::ZeroCopy;
                zeroCopyAttempt.encoderSelection = HardwareEncoderSelector::selectZeroCopyEncoder(
                    config.videoCodec,
                    zeroCopyAttempt.backend
                );
                zeroCopyAttempt.encoderAccepted = zeroCopyAttempt.encoderSelection.zeroCopy;

                if (zeroCopyAttempt.encoderAccepted) {
                    zeroCopyAttempt.score = zeroCopyAttempt.backend.supportsZeroCopyFilter
                        ? kScoreHardwareDecodeFilterEncode
                        : kScoreHardwareDecodeDirectEncode;

                    zeroCopyAttempt.reason = "accepted full hardware path: hardware decode + "
                        + std::string(zeroCopyAttempt.backend.supportsZeroCopyFilter
                            ? "hardware filter + "
                            : "direct hardware frame + ")
                        + "hardware encode";
                }
                else {
                    zeroCopyAttempt.reason = zeroCopyAttempt.encoderSelection.diagnostic.empty()
                        ? "no zero-copy encoder supports backend hardware frames"
                        : zeroCopyAttempt.encoderSelection.diagnostic;
                }

                plan.attempts.emplace_back(zeroCopyAttempt);
                logAttempt(plan.attempts.back());

                if (zeroCopyAttempt.encoderAccepted &&
                    (!bestAttempt.has_value() || betterAttempt(zeroCopyAttempt, *bestAttempt))) {
                    bestAttempt = zeroCopyAttempt;
                }
            }
            else {
                HardwarePipelinePlanAttempt blockedAttempt = baseAttempt;
                blockedAttempt.executionMode = VideoExecutionMode::ZeroCopy;
                blockedAttempt.reason = filterBlockReason;
                plan.attempts.emplace_back(blockedAttempt);
                logAttempt(plan.attempts.back());
            }

            if (!plan.allowFallback) {
                continue;
            }

            HardwarePipelinePlanAttempt mixedEncodeAttempt = baseAttempt;
            mixedEncodeAttempt.executionMode = VideoExecutionMode::MixedGpu;
            mixedEncodeAttempt.encoderSelection = HardwareEncoderSelector::selectMixedGpuEncoder(
                config.videoCodec,
                mixedEncodeAttempt.backend
            );
            mixedEncodeAttempt.encoderAccepted = mixedEncodeAttempt.encoderSelection.encoder &&
                mixedEncodeAttempt.encoderSelection.hardwareEncoder;

            if (mixedEncodeAttempt.encoderAccepted) {
                mixedEncodeAttempt.score = kScoreHardwareDecodeHardwareEncode;
                mixedEncodeAttempt.reason = "accepted mixed hardware path: hardware decode + software filter + hardware encode";

                if (!bestAttempt.has_value() || betterAttempt(mixedEncodeAttempt, *bestAttempt)) {
                    bestAttempt = mixedEncodeAttempt;
                }
            }
            else {
                mixedEncodeAttempt.reason = mixedEncodeAttempt.encoderSelection.diagnostic.empty()
                    ? "no hardware encoder accepts software-frame fallback input"
                    : mixedEncodeAttempt.encoderSelection.diagnostic;
            }

            plan.attempts.emplace_back(mixedEncodeAttempt);
            logAttempt(plan.attempts.back());

            const VideoEncoderSelection genericEncoder = VideoEncoderSelector::select(config.videoCodec);
            HardwarePipelinePlanAttempt hardwareDecodeAttempt = baseAttempt;
            hardwareDecodeAttempt.executionMode = VideoExecutionMode::MixedGpu;
            hardwareDecodeAttempt.encoderSelection = makeGenericEncoderSelection(
                genericEncoder,
                hardwareDecodeAttempt.backend
            );
            hardwareDecodeAttempt.encoderAccepted = hardwareDecodeAttempt.encoderSelection.encoder != nullptr;

            if (hardwareDecodeAttempt.encoderAccepted) {
                hardwareDecodeAttempt.score = kScoreHardwareDecodeOnly;
                hardwareDecodeAttempt.reason = "accepted hardware decode path: hardware decode + software filter + generic encode";

                if (!bestAttempt.has_value() || betterAttempt(hardwareDecodeAttempt, *bestAttempt)) {
                    bestAttempt = hardwareDecodeAttempt;
                }
            }
            else {
                hardwareDecodeAttempt.reason = "hardware decode path rejected: " + genericEncoder.diagnostic;
            }

            plan.attempts.emplace_back(hardwareDecodeAttempt);
            logAttempt(plan.attempts.back());
        }

        if (bestAttempt.has_value()) {
            applySelectedAttemptToPlan(*bestAttempt, plan);
            if (plan.executionMode == VideoExecutionMode::ZeroCopy) {
                spdlog::info("[PLAN] selected: {}", plan.diagnostic);
            }
            else {
                spdlog::warn("[PLAN] selected lower-score hardware path: {}", plan.diagnostic);
            }
            return plan;
        }

        plan.valid = false;
        plan.zeroCopy = false;
        plan.executionMode = VideoExecutionMode::Cpu;

        std::ostringstream oss;
        oss << "hardware pipeline unavailable for codec="
            << videoCodecName(config.videoCodec)
            << "; tested backends=";

        bool first = true;
        for (const HardwarePipelinePlanAttempt& attempt : plan.attempts) {
            if (!first) {
                oss << " | ";
            }
            first = false;
            oss << hardwareDeviceName(attempt.requestedDeviceType)
                << ": mode=" << executionModeName(attempt.executionMode)
                << ", score=" << attempt.score
                << ", reason=" << attempt.reason;
        }

        plan.diagnostic = oss.str();
        if (plan.allowFallback) {
            spdlog::warn("[PLAN] failed but CPU fallback is allowed: {}", plan.diagnostic);
        }
        else {
            spdlog::error("[PLAN] failed and zero-copy fallback is disabled: {}", plan.diagnostic);
        }
        return plan;
    }

} // namespace media::ffmpeg
