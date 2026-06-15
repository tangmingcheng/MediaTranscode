#include "internal/FFmpegPipelinePlanner.h"

#include "internal/FFmpegHardwareContext.h"

#include "spdlog/spdlog.h"

#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    std::string hardwareDeviceName(HardwareDeviceType type)
    {
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

    void logAttempt(const HardwarePipelinePlanAttempt& attempt)
    {
        spdlog::info(
            "[PLAN] backend={} decoderAccepted={} encoderAccepted={} reason={}",
            hardwareDeviceName(attempt.requestedDeviceType),
            attempt.decoderAccepted,
            attempt.encoderAccepted,
            attempt.reason
        );

        if (!attempt.decoderAccepted) {
            return;
        }

        spdlog::info(
            "[PLAN]   decoder: hw_pix_fmt={}, av_device_type={}",
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
        if (requestedDeviceType != HardwareDeviceType::Auto &&
            requestedDeviceType != HardwareDeviceType::None) {
            return { requestedDeviceType };
        }

#if defined(_WIN32)
        return {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::QSV,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::DRM,
            HardwareDeviceType::VideoToolbox
        };
#elif defined(__APPLE__)
        return {
            HardwareDeviceType::VideoToolbox,
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA
        };
#else
        return {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VideoToolbox
        };
#endif
    }

    HardwarePipelinePlan FFmpegPipelinePlanner::planHardwarePipeline(
        const TranscodeConfig& config,
        const AVCodec* decoder)
    {
        HardwarePipelinePlan plan;
        plan.allowFallback = config.hardware.allowZeroCopyFallback;

        spdlog::info(
            "[PLAN] request: videoCodec={}, preferZeroCopy=true, allowZeroCopyFallback={}, requestedDevice={}",
            videoCodecName(config.videoCodec),
            plan.allowFallback,
            hardwareDeviceName(config.hardware.deviceType)
        );

        if (!decoder) {
            plan.valid = false;
            plan.diagnostic = "hardware pipeline planning failed: decoder is null";
            spdlog::error("[PLAN] {}", plan.diagnostic);
            return plan;
        }

        const std::vector<HardwareDeviceType> priority = backendPriority(config.hardware.deviceType);

        for (HardwareDeviceType deviceType : priority) {
            HardwarePipelinePlanAttempt attempt;
            attempt.requestedDeviceType = deviceType;
            attempt.decoderConfig = HardwareDecoderSupport::findConfig(decoder, deviceType);

            if (!attempt.decoderConfig.valid) {
                attempt.decoderAccepted = false;
                attempt.reason = "decoder does not support backend hardware frames";
                plan.attempts.emplace_back(attempt);
                logAttempt(plan.attempts.back());
                continue;
            }

            attempt.decoderAccepted = true;
            attempt.backend = HardwareBackendRegistry::profileFor(attempt.decoderConfig.deviceType);

            if (attempt.backend.deviceType == HardwareDeviceType::None ||
                attempt.backend.hardwarePixelFormat == AV_PIX_FMT_NONE) {
                attempt.reason = "backend profile is not available";
                plan.attempts.emplace_back(attempt);
                logAttempt(plan.attempts.back());
                continue;
            }

            attempt.encoderSelection = HardwareEncoderSelector::select(
                config.videoCodec,
                attempt.backend,
                true
            );

            attempt.encoderAccepted = attempt.encoderSelection.zeroCopy;
            if (attempt.encoderAccepted) {
                attempt.reason = "accepted zero-copy path: decoder and encoder share hardware pixel format " +
                    pixelFormatName(attempt.backend.hardwarePixelFormat);
                plan.attempts.emplace_back(attempt);
                logAttempt(plan.attempts.back());

                plan.valid = true;
                plan.zeroCopy = true;
                plan.backend = attempt.backend;
                plan.decoderConfig = attempt.decoderConfig;
                plan.encoderSelection = attempt.encoderSelection;
                plan.diagnostic = "selected zero-copy backend=" + hardwareDeviceName(plan.backend.deviceType) +
                    ", encoder=" + plan.encoderSelection.encoderName +
                    ", hw_pix_fmt=" + pixelFormatName(plan.backend.hardwarePixelFormat);
                spdlog::info("[PLAN] selected: {}", plan.diagnostic);
                return plan;
            }

            attempt.reason = attempt.encoderSelection.diagnostic.empty()
                ? "no zero-copy encoder supports backend hardware frames"
                : attempt.encoderSelection.diagnostic;

            plan.attempts.emplace_back(attempt);
            logAttempt(plan.attempts.back());
        }

        plan.valid = false;
        plan.zeroCopy = false;

        std::ostringstream oss;
        oss << "zero-copy hardware pipeline unavailable for codec="
            << videoCodecName(config.videoCodec)
            << "; tested backends=";

        bool first = true;
        for (const HardwarePipelinePlanAttempt& attempt : plan.attempts) {
            if (!first) {
                oss << " | ";
            }
            first = false;
            oss << hardwareDeviceName(attempt.requestedDeviceType) << ": " << attempt.reason;
        }

        plan.diagnostic = oss.str();
        if (plan.allowFallback) {
            spdlog::warn("[PLAN] failed but fallback is allowed: {}", plan.diagnostic);
        }
        else {
            spdlog::error("[PLAN] failed and fallback is disabled: {}", plan.diagnostic);
        }
        return plan;
    }

} // namespace media::ffmpeg
