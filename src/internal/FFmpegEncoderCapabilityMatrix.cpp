#include "internal/FFmpegEncoderCapabilityMatrix.h"

#include <string>

namespace media::ffmpeg {
namespace {

    bool encoderNameEquals(const AVCodec* encoder, const char* expected)
    {
        return encoder && encoder->name && std::string(encoder->name) == expected;
    }

    bool encoderNameContains(const AVCodec* encoder, const char* token)
    {
        return encoder && encoder->name && std::string(encoder->name).find(token) != std::string::npos;
    }

    std::string x26xPresetName(VideoEncodeSpeedPreset preset)
    {
        switch (preset) {
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

    std::string nvencPresetName(VideoEncodeSpeedPreset preset)
    {
        switch (preset) {
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

    std::string rkmppPresetName(VideoEncodeSpeedPreset preset)
    {
        switch (preset) {
        case VideoEncodeSpeedPreset::Ultrafast:
        case VideoEncodeSpeedPreset::Superfast:
        case VideoEncodeSpeedPreset::Veryfast:
        case VideoEncodeSpeedPreset::Faster:
        case VideoEncodeSpeedPreset::Fast:
            return "fast";
        case VideoEncodeSpeedPreset::Slow:
        case VideoEncodeSpeedPreset::Slower:
        case VideoEncodeSpeedPreset::Veryslow:
        case VideoEncodeSpeedPreset::Placebo:
            return "quality";
        case VideoEncodeSpeedPreset::Medium:
        default:
            return "medium";
        }
    }

    FFmpegEncoderCapabilities baseCapabilities(const AVCodec* encoder)
    {
        FFmpegEncoderCapabilities capabilities;
        capabilities.encoderName = encoder && encoder->name ? encoder->name : "unknown";
        capabilities.family = FFmpegEncoderFamily::Generic;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.supportsCrf = true;
        capabilities.rateControlOptionName = "rc";
        capabilities.qualityOptionName = "crf";
        return capabilities;
    }

    void configureX26x(FFmpegEncoderCapabilities& capabilities,
                       FFmpegEncoderFamily family)
    {
        capabilities.family = family;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(family);
        capabilities.hardware = false;
        capabilities.supportsCbr = true;
        capabilities.supportsVbr = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName.clear();
        capabilities.qualityOptionName = "crf";
        capabilities.presetOptionName = "preset";
        capabilities.supportsNalHrdCbr = true;
    }

    void configureNvenc(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::NVENC;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCbr = true;
        capabilities.supportsVbr = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName = "rc";
        capabilities.qualityOptionName = "cq";
        capabilities.presetOptionName = "preset";
    }

    void configureRkmpp(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::RKMPP;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCbr = true;
        capabilities.supportsVbr = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName = "rc_mode";
        capabilities.qualityOptionName = "qp_init";
        capabilities.presetOptionName = "preset";
    }

    void configureMediaFoundation(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::MediaFoundation;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCrf = false;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName.clear();
        capabilities.qualityOptionName.clear();
        capabilities.presetOptionName.clear();
        capabilities.supportsPrivateVbvOptions = false;
    }

    void configureQsv(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::QSV;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName.clear();
        capabilities.qualityOptionName = "global_quality";
        capabilities.presetOptionName = "preset";
    }

    void configureVaapi(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::VAAPI;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName = "rc_mode";
        capabilities.qualityOptionName = "qp";
        capabilities.presetOptionName.clear();
    }

    void configureAmf(FFmpegEncoderCapabilities& capabilities)
    {
        capabilities.family = FFmpegEncoderFamily::AMF;
        capabilities.familyName = FFmpegEncoderCapabilityMatrix::familyName(capabilities.family);
        capabilities.hardware = true;
        capabilities.supportsCrf = true;
        capabilities.supportsCappedVbr = true;
        capabilities.rateControlOptionName = "rc";
        capabilities.qualityOptionName = "qp_i";
        capabilities.presetOptionName = "quality";
    }

} // namespace

    FFmpegEncoderCapabilities FFmpegEncoderCapabilityMatrix::query(const AVCodec* encoder)
    {
        FFmpegEncoderCapabilities capabilities = baseCapabilities(encoder);

        if (!encoder || !encoder->name) {
            return capabilities;
        }

        if (encoderNameEquals(encoder, "libx264")) {
            configureX26x(capabilities, FFmpegEncoderFamily::X264);
        }
        else if (encoderNameEquals(encoder, "libx265")) {
            configureX26x(capabilities, FFmpegEncoderFamily::X265);
        }
        else if (encoderNameContains(encoder, "_nvenc")) {
            configureNvenc(capabilities);
        }
        else if (encoderNameContains(encoder, "_rkmpp")) {
            configureRkmpp(capabilities);
        }
        else if (encoderNameContains(encoder, "_mf")) {
            configureMediaFoundation(capabilities);
        }
        else if (encoderNameContains(encoder, "_qsv")) {
            configureQsv(capabilities);
        }
        else if (encoderNameContains(encoder, "_vaapi")) {
            configureVaapi(capabilities);
        }
        else if (encoderNameContains(encoder, "_amf")) {
            configureAmf(capabilities);
        }

        return capabilities;
    }

    bool FFmpegEncoderCapabilityMatrix::supportsRateControl(const FFmpegEncoderCapabilities& capabilities,
                                                            VideoRateControlMode mode)
    {
        switch (mode) {
        case VideoRateControlMode::CBR:
            return capabilities.supportsCbr;
        case VideoRateControlMode::VBR:
            return capabilities.supportsVbr;
        case VideoRateControlMode::CRF:
            return capabilities.supportsCrf;
        case VideoRateControlMode::CappedVBR:
            return capabilities.supportsCappedVbr;
        case VideoRateControlMode::Auto:
        default:
            return true;
        }
    }

    std::string FFmpegEncoderCapabilityMatrix::rateControlValue(const FFmpegEncoderCapabilities& capabilities,
                                                                VideoRateControlMode mode)
    {
        switch (capabilities.family) {
        case FFmpegEncoderFamily::RKMPP:
            switch (mode) {
            case VideoRateControlMode::CBR:
                return "CBR";
            case VideoRateControlMode::VBR:
            case VideoRateControlMode::CappedVBR:
                return "VBR";
            case VideoRateControlMode::CRF:
                return "CQP";
            case VideoRateControlMode::Auto:
            default:
                return {};
            }

        case FFmpegEncoderFamily::NVENC:
            switch (mode) {
            case VideoRateControlMode::CBR:
                return "cbr";
            case VideoRateControlMode::VBR:
            case VideoRateControlMode::CRF:
            case VideoRateControlMode::CappedVBR:
                return "vbr";
            case VideoRateControlMode::Auto:
            default:
                return {};
            }

        case FFmpegEncoderFamily::VAAPI:
            switch (mode) {
            case VideoRateControlMode::CBR:
                return "CBR";
            case VideoRateControlMode::VBR:
            case VideoRateControlMode::CappedVBR:
                return "VBR";
            case VideoRateControlMode::CRF:
                return "CQP";
            case VideoRateControlMode::Auto:
            default:
                return {};
            }

        case FFmpegEncoderFamily::AMF:
            switch (mode) {
            case VideoRateControlMode::CBR:
                return "cbr";
            case VideoRateControlMode::VBR:
            case VideoRateControlMode::CappedVBR:
                return "vbr_peak";
            case VideoRateControlMode::CRF:
                return "cqp";
            case VideoRateControlMode::Auto:
            default:
                return {};
            }

        case FFmpegEncoderFamily::X264:
        case FFmpegEncoderFamily::X265:
        case FFmpegEncoderFamily::QSV:
        case FFmpegEncoderFamily::MediaFoundation:
        case FFmpegEncoderFamily::Generic:
        case FFmpegEncoderFamily::Unknown:
        default:
            switch (mode) {
            case VideoRateControlMode::CBR:
                return "cbr";
            case VideoRateControlMode::VBR:
            case VideoRateControlMode::CappedVBR:
                return "vbr";
            case VideoRateControlMode::CRF:
                return "crf";
            case VideoRateControlMode::Auto:
            default:
                return {};
            }
        }
    }

    std::string FFmpegEncoderCapabilityMatrix::presetValue(const FFmpegEncoderCapabilities& capabilities,
                                                           VideoEncodeSpeedPreset preset)
    {
        switch (capabilities.family) {
        case FFmpegEncoderFamily::X264:
        case FFmpegEncoderFamily::X265:
            return x26xPresetName(preset);

        case FFmpegEncoderFamily::NVENC:
            return nvencPresetName(preset);

        case FFmpegEncoderFamily::RKMPP:
            return rkmppPresetName(preset);

        case FFmpegEncoderFamily::QSV:
            return x26xPresetName(preset);

        case FFmpegEncoderFamily::AMF:
            switch (preset) {
            case VideoEncodeSpeedPreset::Ultrafast:
            case VideoEncodeSpeedPreset::Superfast:
            case VideoEncodeSpeedPreset::Veryfast:
            case VideoEncodeSpeedPreset::Faster:
            case VideoEncodeSpeedPreset::Fast:
                return "speed";
            case VideoEncodeSpeedPreset::Slow:
            case VideoEncodeSpeedPreset::Slower:
            case VideoEncodeSpeedPreset::Veryslow:
            case VideoEncodeSpeedPreset::Placebo:
                return "quality";
            case VideoEncodeSpeedPreset::Medium:
            default:
                return "balanced";
            }

        case FFmpegEncoderFamily::MediaFoundation:
        case FFmpegEncoderFamily::VAAPI:
        case FFmpegEncoderFamily::Generic:
        case FFmpegEncoderFamily::Unknown:
        default:
            return {};
        }
    }

    std::string FFmpegEncoderCapabilityMatrix::familyName(FFmpegEncoderFamily family)
    {
        switch (family) {
        case FFmpegEncoderFamily::X264:
            return "x264";
        case FFmpegEncoderFamily::X265:
            return "x265";
        case FFmpegEncoderFamily::NVENC:
            return "nvenc";
        case FFmpegEncoderFamily::RKMPP:
            return "rkmpp";
        case FFmpegEncoderFamily::MediaFoundation:
            return "mediafoundation";
        case FFmpegEncoderFamily::QSV:
            return "qsv";
        case FFmpegEncoderFamily::VAAPI:
            return "vaapi";
        case FFmpegEncoderFamily::AMF:
            return "amf";
        case FFmpegEncoderFamily::Generic:
            return "generic";
        case FFmpegEncoderFamily::Unknown:
        default:
            return "unknown";
        }
    }

    std::string FFmpegEncoderCapabilityMatrix::rateControlName(VideoRateControlMode mode)
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

    bool FFmpegEncoderCapabilityMatrix::isRateControlPrivateOption(const std::string& name)
    {
        return name == "rc" ||
            name == "rate_control" ||
            name == "rc_mode" ||
            name == "nal-hrd";
    }

} // namespace media::ffmpeg
