#include "internal/FFmpegHardwareEncoderSelector.h"

#include "internal/FFmpegUtils.h"

#include <array>
#include <sstream>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    using EncoderNameList = std::array<const char*, 5>;

    const AVPixelFormat kSoftwarePixelFormatSentinel = AV_PIX_FMT_NONE;

    EncoderNameList encoderCandidates(VideoCodec codec, HardwareDeviceType deviceType)
    {
        switch (deviceType) {
        case HardwareDeviceType::D3D11VA:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_nvenc", "h264_amf", "h264_mf", nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_nvenc", "hevc_amf", "hevc_mf", nullptr, nullptr };
            case VideoCodec::AV1:
                return { "av1_nvenc", "av1_amf", "av1_mf", nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::CUDA:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_nvenc", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_nvenc", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::AV1:
                return { "av1_nvenc", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::QSV:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_qsv", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_qsv", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::AV1:
                return { "av1_qsv", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::VAAPI:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_vaapi", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_vaapi", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::RKMPP:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_rkmpp", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_rkmpp", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::DRM:
            return { nullptr, nullptr, nullptr, nullptr, nullptr };

        case HardwareDeviceType::VideoToolbox:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_videotoolbox", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_videotoolbox", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            return { nullptr, nullptr, nullptr, nullptr, nullptr };
        }
    }

#if LIBAVCODEC_VERSION_MAJOR >= 61
    const AVPixelFormat* supportedPixelFormats(const AVCodec* encoder, int* count)
    {
        if (count) {
            *count = 0;
        }

        if (!encoder) {
            return nullptr;
        }

        const void* configs = nullptr;
        int configCount = 0;
        const int ret = avcodec_get_supported_config(
            nullptr,
            encoder,
            AV_CODEC_CONFIG_PIX_FORMAT,
            0,
            &configs,
            &configCount
        );

        if (ret < 0 || !configs || configCount <= 0) {
            return nullptr;
        }

        if (count) {
            *count = configCount;
        }

        return static_cast<const AVPixelFormat*>(configs);
    }
#endif

    bool pixelFormatListContains(const AVPixelFormat* formats,
                                 int count,
                                 AVPixelFormat target)
    {
        if (!formats || target == AV_PIX_FMT_NONE) {
            return false;
        }

        if (count >= 0) {
            for (int i = 0; i < count; ++i) {
                if (formats[i] == target) {
                    return true;
                }
            }
            return false;
        }

        for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == target) {
                return true;
            }
        }

        return false;
    }

    bool isSoftwareFramePixelFormat(AVPixelFormat format)
    {
        switch (format) {
        case AV_PIX_FMT_NONE:
        case AV_PIX_FMT_D3D11:
        case AV_PIX_FMT_CUDA:
        case AV_PIX_FMT_QSV:
        case AV_PIX_FMT_VAAPI:
        case AV_PIX_FMT_DRM_PRIME:
        case AV_PIX_FMT_VIDEOTOOLBOX:
            return false;
        default:
            return true;
        }
    }

    std::string pixelFormatName(AVPixelFormat format)
    {
        const char* name = av_get_pix_fmt_name(format);
        return name ? name : "unknown";
    }

    std::string pixelFormatListText(const AVCodec* encoder)
    {
        if (!encoder) {
            return "none";
        }

        std::ostringstream oss;
        bool first = true;

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &formatCount);
        if (!formats || formatCount <= 0) {
            return "unknown";
        }

        for (int i = 0; i < formatCount; ++i) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << pixelFormatName(formats[i]);
        }
#else
        if (!encoder->pix_fmts) {
            return "unknown";
        }

        for (const AVPixelFormat* p = encoder->pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << pixelFormatName(*p);
        }
#endif

        return oss.str();
    }

    AVPixelFormat firstSupportedPreferredFormat(const AVCodec* encoder,
                                                const std::vector<AVPixelFormat>& preferred)
    {
        if (!encoder) {
            return AV_PIX_FMT_NONE;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &formatCount);
        if (formats && formatCount > 0) {
            for (AVPixelFormat format : preferred) {
                if (pixelFormatListContains(formats, formatCount, format)) {
                    return format;
                }
            }
            return formats[0];
        }
#else
        if (encoder->pix_fmts) {
            for (AVPixelFormat format : preferred) {
                if (pixelFormatListContains(encoder->pix_fmts, -1, format)) {
                    return format;
                }
            }
            return encoder->pix_fmts[0];
        }
#endif

        return preferred.empty() ? AV_PIX_FMT_NONE : preferred.front();
    }

    HardwareEncoderSelection makeSelection(const AVCodec* encoder,
                                           const char* encoderName,
                                           const HardwareBackendProfile& backend,
                                           bool zeroCopy,
                                           std::vector<HardwareEncoderCandidate> candidates,
                                           std::string diagnostic)
    {
        HardwareEncoderSelection selection;
        selection.backend = backend;
        selection.encoder = encoder;
        selection.encoderName = encoderName ? encoderName : "";
        selection.hardwareEncoder = isHardwareEncoderName(encoderName);
        selection.zeroCopy = zeroCopy;
        selection.pixelFormat = zeroCopy
            ? backend.hardwarePixelFormat
            : HardwareEncoderSelector::chooseFallbackSoftwarePixelFormat(encoder, backend.deviceType);
        selection.candidates = std::move(candidates);
        selection.diagnostic = std::move(diagnostic);
        return selection;
    }

    bool backendCanFeedEncoderHardwareFrames(const HardwareBackendProfile& backend,
                                             const AVCodec* encoder)
    {
        const bool hasZeroCopyTransport =
            backend.supportsZeroCopyFilter ||
            backend.supportsDirectHardwareFrameEncode;

        return hasZeroCopyTransport &&
            backend.hardwarePixelFormat != AV_PIX_FMT_NONE &&
            HardwareEncoderSelector::encoderSupportsPixelFormat(encoder, backend.hardwarePixelFormat);
    }

} // namespace

    HardwareEncoderSelection HardwareEncoderSelector::selectZeroCopyEncoder(
        VideoCodec codec,
        const HardwareBackendProfile& backend)
    {
        HardwareEncoderSelection selection = select(codec, backend, true);
        if (selection.zeroCopy) {
            return selection;
        }

        selection.encoder = nullptr;
        selection.encoderName.clear();
        selection.pixelFormat = AV_PIX_FMT_NONE;
        selection.hardwareEncoder = false;
        selection.diagnostic = "no zero-copy encoder accepted backend hardware frames";
        return selection;
    }

    HardwareEncoderSelection HardwareEncoderSelector::selectMixedGpuEncoder(
        VideoCodec codec,
        const HardwareBackendProfile& backend)
    {
        const EncoderNameList names = encoderCandidates(codec, backend.deviceType);
        std::vector<HardwareEncoderCandidate> candidates;

        for (const char* encoderName : names) {
            if (!encoderName) {
                continue;
            }

            HardwareEncoderCandidate candidate;
            candidate.encoderName = encoderName;

            const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName);
            if (!encoder) {
                candidate.available = false;
                candidate.hardwareEncoder = isHardwareEncoderName(encoderName);
                candidate.supportsBackendHardwareFrames = false;
                candidate.pixelFormats = "none";
                candidate.rejectionReason = "encoder is not available in current FFmpeg build";
                candidates.emplace_back(std::move(candidate));
                continue;
            }

            const bool hardwareEncoder = isHardwareEncoderName(encoderName);
            const AVPixelFormat softwareInputFormat = chooseFallbackSoftwarePixelFormat(
                encoder,
                backend.deviceType
            );
            const bool supportsSoftwareInput = isSoftwareFramePixelFormat(softwareInputFormat);
            const bool canUseBackendHardwareFrames = backendCanFeedEncoderHardwareFrames(backend, encoder);

            candidate.available = true;
            candidate.hardwareEncoder = hardwareEncoder;
            candidate.supportsBackendHardwareFrames = canUseBackendHardwareFrames;
            candidate.pixelFormats = pixelFormatListText(encoder);

            if (hardwareEncoder && supportsSoftwareInput) {
                candidate.rejectionReason = "accepted mixed GPU fallback: hardware encoder accepts software pixel format " +
                    pixelFormatName(softwareInputFormat);
                candidates.emplace_back(std::move(candidate));

                HardwareEncoderSelection selection = makeSelection(
                    encoder,
                    encoderName,
                    backend,
                    false,
                    std::move(candidates),
                    "selected mixed GPU encoder " + std::string(encoderName)
                );
                selection.pixelFormat = softwareInputFormat;
                return selection;
            }

            if (!hardwareEncoder) {
                candidate.rejectionReason = "encoder is not classified as hardware encoder";
            }
            else {
                candidate.rejectionReason = "hardware encoder does not accept software-frame fallback pixel format";
            }

            candidates.emplace_back(std::move(candidate));
        }

        HardwareEncoderSelection selection;
        selection.backend = backend;
        selection.candidates = std::move(candidates);
        selection.diagnostic = "no mixed GPU encoder accepts software-frame fallback input for backend " +
            std::string(backend.name ? backend.name : "unknown");
        return selection;
    }

    HardwareEncoderSelection HardwareEncoderSelector::select(VideoCodec codec,
                                                             const HardwareBackendProfile& backend,
                                                             bool preferZeroCopy)
    {
        const EncoderNameList names = encoderCandidates(codec, backend.deviceType);

        const AVCodec* firstAvailable = nullptr;
        const char* firstAvailableName = nullptr;
        std::vector<HardwareEncoderCandidate> candidates;

        for (const char* encoderName : names) {
            if (!encoderName) {
                continue;
            }

            HardwareEncoderCandidate candidate;
            candidate.encoderName = encoderName;

            const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName);
            if (!encoder) {
                candidate.available = false;
                candidate.hardwareEncoder = isHardwareEncoderName(encoderName);
                candidate.supportsBackendHardwareFrames = false;
                candidate.pixelFormats = "none";
                candidate.rejectionReason = "encoder is not available in current FFmpeg build";
                candidates.emplace_back(std::move(candidate));
                continue;
            }

            if (!firstAvailable) {
                firstAvailable = encoder;
                firstAvailableName = encoderName;
            }

            const bool canUseBackendHardwareFrames = backendCanFeedEncoderHardwareFrames(backend, encoder);

            candidate.available = true;
            candidate.hardwareEncoder = isHardwareEncoderName(encoderName);
            candidate.supportsBackendHardwareFrames = canUseBackendHardwareFrames;
            candidate.pixelFormats = pixelFormatListText(encoder);

            if (canUseBackendHardwareFrames) {
                candidate.rejectionReason = "accepted: encoder supports backend hardware pixel format " +
                    pixelFormatName(backend.hardwarePixelFormat);
                candidates.emplace_back(std::move(candidate));
                if (preferZeroCopy) {
                    return makeSelection(
                        encoder,
                        encoderName,
                        backend,
                        true,
                        std::move(candidates),
                        "selected zero-copy encoder " + std::string(encoderName)
                    );
                }
                continue;
            }

            candidate.rejectionReason = "encoder does not support backend hardware pixel format " +
                pixelFormatName(backend.hardwarePixelFormat);
            candidates.emplace_back(std::move(candidate));
        }

        if (firstAvailable) {
            return makeSelection(
                firstAvailable,
                firstAvailableName,
                backend,
                false,
                std::move(candidates),
                "no zero-copy encoder accepted backend hardware frames; fallback encoder available " +
                    std::string(firstAvailableName)
            );
        }

        HardwareEncoderSelection selection;
        selection.backend = backend;
        selection.candidates = std::move(candidates);
        selection.diagnostic = "no encoder candidates are available for backend " +
            std::string(backend.name ? backend.name : "unknown");
        return selection;
    }

    bool HardwareEncoderSelector::encoderSupportsPixelFormat(const AVCodec* encoder,
                                                            AVPixelFormat pixelFormat)
    {
        if (!encoder || pixelFormat == AV_PIX_FMT_NONE) {
            return false;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &formatCount);
        if (!formats || formatCount <= 0) {
            return false;
        }

        return pixelFormatListContains(formats, formatCount, pixelFormat);
#else
        if (!encoder->pix_fmts) {
            return false;
        }

        return pixelFormatListContains(encoder->pix_fmts, -1, pixelFormat);
#endif
    }

    AVPixelFormat HardwareEncoderSelector::chooseFallbackSoftwarePixelFormat(
        const AVCodec* encoder,
        HardwareDeviceType deviceType)
    {
        std::vector<AVPixelFormat> preferred;

        switch (deviceType) {
        case HardwareDeviceType::D3D11VA:
        case HardwareDeviceType::DRM:
        case HardwareDeviceType::RKMPP:
        case HardwareDeviceType::VideoToolbox:
            preferred = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
            break;

        case HardwareDeviceType::CUDA:
        case HardwareDeviceType::QSV:
        case HardwareDeviceType::VAAPI:
            preferred = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
            break;

        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            preferred = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12 };
            break;
        }

        AVPixelFormat selected = firstSupportedPreferredFormat(encoder, preferred);
        if (selected == kSoftwarePixelFormatSentinel) {
            selected = chooseVideoEncoderPixelFormat(encoder);
        }

        return selected;
    }

    const char* HardwareEncoderSelector::firstAvailableEncoder(VideoCodec codec,
                                                               HardwareDeviceType deviceType)
    {
        const EncoderNameList names = encoderCandidates(codec, deviceType);
        for (const char* name : names) {
            if (!name) {
                continue;
            }

            if (avcodec_find_encoder_by_name(name)) {
                return name;
            }
        }

        return nullptr;
    }

} // namespace media::ffmpeg
