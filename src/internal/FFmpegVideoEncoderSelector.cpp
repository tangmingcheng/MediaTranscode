#include "internal/FFmpegVideoEncoderSelector.h"

#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <sstream>
#include <string>

extern "C" {
#include <libavcodec/version_major.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    AVCodecID normalizeCodecId(AVCodecID codecId)
    {
        return codecId;
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

    const char* pixelFormatName(AVPixelFormat format)
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
        int count = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &count);
        if (!formats || count <= 0) {
            return "unknown";
        }

        for (int i = 0; i < count; ++i) {
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

    bool encoderIsExperimental(const AVCodec* encoder)
    {
        return encoder && ((encoder->capabilities & AV_CODEC_CAP_EXPERIMENTAL) != 0);
    }

    bool encoderIsHardware(const AVCodec* encoder)
    {
        if (!encoder) {
            return false;
        }

#ifdef AV_CODEC_CAP_HARDWARE
        if ((encoder->capabilities & AV_CODEC_CAP_HARDWARE) != 0) {
            return true;
        }
#endif

        return isHardwareEncoderName(encoder->name);
    }

    bool encoderSupportsFrameThreads(const AVCodec* encoder)
    {
        return encoder && ((encoder->capabilities & AV_CODEC_CAP_FRAME_THREADS) != 0);
    }

    bool encoderSupportsSliceThreads(const AVCodec* encoder)
    {
        return encoder && ((encoder->capabilities & AV_CODEC_CAP_SLICE_THREADS) != 0);
    }

    bool encoderNameEquals(const AVCodec* encoder, const char* expected)
    {
        return encoder && encoder->name && std::string(encoder->name) == expected;
    }

    int softwareEncoderPriority(VideoCodec codec, const AVCodec* encoder)
    {
        if (!encoder || encoderIsHardware(encoder)) {
            return 0;
        }

        switch (codec) {
        case VideoCodec::H264:
            if (encoderNameEquals(encoder, "libx264")) {
                return 1000;
            }
            if (encoderNameEquals(encoder, "libopenh264")) {
                return 700;
            }
            if (encoderNameEquals(encoder, "h264")) {
                return 500;
            }
            return 0;

        case VideoCodec::H265:
            if (encoderNameEquals(encoder, "libx265")) {
                return 1000;
            }
            if (encoderNameEquals(encoder, "hevc")) {
                return 500;
            }
            return 0;

        case VideoCodec::MPEG4:
            if (encoderNameEquals(encoder, "mpeg4")) {
                return 1000;
            }
            return 0;

        case VideoCodec::VP8:
            if (encoderNameEquals(encoder, "libvpx")) {
                return 1000;
            }
            if (encoderNameEquals(encoder, "vp8")) {
                return 500;
            }
            return 0;

        case VideoCodec::VP9:
            if (encoderNameEquals(encoder, "libvpx-vp9")) {
                return 1000;
            }
            if (encoderNameEquals(encoder, "vp9")) {
                return 500;
            }
            return 0;

        case VideoCodec::AV1:
            if (encoderNameEquals(encoder, "libsvtav1")) {
                return 1000;
            }
            if (encoderNameEquals(encoder, "libaom-av1")) {
                return 900;
            }
            if (encoderNameEquals(encoder, "librav1e")) {
                return 700;
            }
            if (encoderNameEquals(encoder, "av1")) {
                return 500;
            }
            return 0;

        case VideoCodec::Copy:
        default:
            return 0;
        }
    }

    bool allowedByCpuEncoderPolicy(VideoCodec codec, const AVCodec* encoder)
    {
        if (!encoder || encoderIsHardware(encoder)) {
            return false;
        }

        switch (codec) {
        case VideoCodec::H265:
            // libkvazaar can produce streams that some Windows HEVC decoders render incorrectly.
            // CPU HEVC transcode should use the de-facto standard software encoder only.
            return encoderNameEquals(encoder, "libx265") || encoderNameEquals(encoder, "hevc");

        default:
            return softwareEncoderPriority(codec, encoder) > 0;
        }
    }

    int scoreEncoder(const AVCodec* encoder,
                     VideoCodec codec,
                     bool preferHardwareEncoder)
    {
        if (!encoder) {
            return -100000;
        }

        const bool hardware = encoderIsHardware(encoder);
        int score = 0;

        if (preferHardwareEncoder) {
            score += hardware ? 600 : 100;
        }
        else {
            score += hardware ? 100 : 600;
        }

        score += softwareEncoderPriority(codec, encoder);

        if (!encoderIsExperimental(encoder)) {
            score += 100;
        }
        else {
            score -= 200;
        }

        if (encoderSupportsFrameThreads(encoder)) {
            score += 40;
        }

        if (encoderSupportsSliceThreads(encoder)) {
            score += 20;
        }

        if (encoder->name && encoder->long_name) {
            score += 10;
        }

        return score;
    }

    std::string describeReason(const AVCodec* encoder,
                               VideoCodec codec,
                               int score,
                               AVPixelFormat pixelFormat,
                               bool preferHardwareEncoder,
                               bool allowHardwareEncoder)
    {
        std::ostringstream oss;
        oss << "score=" << score
            << ", prefer_hardware=" << preferHardwareEncoder
            << ", allow_hardware=" << allowHardwareEncoder
            << ", hardware=" << encoderIsHardware(encoder)
            << ", software_priority=" << softwareEncoderPriority(codec, encoder)
            << ", experimental=" << encoderIsExperimental(encoder)
            << ", frame_threads=" << encoderSupportsFrameThreads(encoder)
            << ", slice_threads=" << encoderSupportsSliceThreads(encoder)
            << ", selected_pix_fmt=" << pixelFormatName(pixelFormat);
        return oss.str();
    }

    bool betterCandidate(const VideoEncoderCandidate& lhs,
                         const VideoEncoderCandidate& rhs)
    {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }

        return lhs.name < rhs.name;
    }

} // namespace

    AVCodecID VideoEncoderSelector::codecIdFor(VideoCodec codec)
    {
        switch (codec) {
        case VideoCodec::H264:
            return AV_CODEC_ID_H264;
        case VideoCodec::H265:
            return AV_CODEC_ID_HEVC;
        case VideoCodec::MPEG4:
            return AV_CODEC_ID_MPEG4;
        case VideoCodec::VP8:
            return AV_CODEC_ID_VP8;
        case VideoCodec::VP9:
            return AV_CODEC_ID_VP9;
        case VideoCodec::AV1:
            return AV_CODEC_ID_AV1;
        case VideoCodec::Copy:
        default:
            return AV_CODEC_ID_NONE;
        }
    }

    VideoEncoderSelection VideoEncoderSelector::select(VideoCodec codec,
                                                       bool preferHardwareEncoder,
                                                       bool allowHardwareEncoder)
    {
        VideoEncoderSelection selection;
        const AVCodecID targetCodecId = codecIdFor(codec);

        if (targetCodecId == AV_CODEC_ID_NONE) {
            selection.diagnostic = "video encoder selection failed: unsupported target codec";
            return selection;
        }

        const AVCodec* bestEncoder = nullptr;
        VideoEncoderCandidate bestCandidate;

        void* iterator = nullptr;
        const AVCodec* encoder = nullptr;
        while ((encoder = av_codec_iterate(&iterator)) != nullptr) {
            if (!av_codec_is_encoder(encoder)) {
                continue;
            }

            if (normalizeCodecId(encoder->id) != targetCodecId) {
                continue;
            }

            VideoEncoderCandidate candidate;
            candidate.encoder = encoder;
            candidate.name = encoder->name ? encoder->name : "unknown";
            candidate.codecId = encoder->id;
            candidate.hardwareEncoder = encoderIsHardware(encoder);
            candidate.experimental = encoderIsExperimental(encoder);
            candidate.selectedPixelFormat = chooseVideoEncoderPixelFormat(encoder);
            candidate.pixelFormats = pixelFormatListText(encoder);

            if (candidate.hardwareEncoder && !allowHardwareEncoder) {
                candidate.score = -100000;
                candidate.reason = "rejected: hardware encoder is disabled by policy";
                selection.candidates.emplace_back(candidate);
                continue;
            }

            if (!allowHardwareEncoder && !allowedByCpuEncoderPolicy(codec, encoder)) {
                candidate.score = -90000;
                candidate.reason = "rejected: encoder is not approved for CPU transcode policy";
                selection.candidates.emplace_back(candidate);
                continue;
            }

            candidate.score = scoreEncoder(encoder, codec, preferHardwareEncoder);
            candidate.reason = describeReason(
                encoder,
                codec,
                candidate.score,
                candidate.selectedPixelFormat,
                preferHardwareEncoder,
                allowHardwareEncoder
            );

            selection.candidates.emplace_back(candidate);

            if (!bestEncoder || betterCandidate(candidate, bestCandidate)) {
                bestEncoder = encoder;
                bestCandidate = candidate;
            }
        }

        std::sort(
            selection.candidates.begin(),
            selection.candidates.end(),
            betterCandidate
        );

        if (!bestEncoder) {
            selection.diagnostic = allowHardwareEncoder
                ? "video encoder selection failed: no encoder available for requested codec"
                : "video encoder selection failed: no approved software encoder available for requested codec while hardware is disabled";
            return selection;
        }

        selection.encoder = bestEncoder;
        selection.encoderName = bestCandidate.name;
        selection.pixelFormat = bestCandidate.selectedPixelFormat;
        selection.diagnostic = "selected encoder " + bestCandidate.name + ": " + bestCandidate.reason;
        return selection;
    }

} // namespace media::ffmpeg
