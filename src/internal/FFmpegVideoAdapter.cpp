#include "internal/FFmpegUtils.h"

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
}

namespace media::ffmpeg {
namespace {

#if LIBAVCODEC_VERSION_MAJOR >= 61
    const AVPixelFormat* getSupportedPixelFormats(const AVCodec* encoder, int* count)
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

} // namespace

    int normalizeEvenSize(int value)
    {
        if (value <= 0) {
            return value;
        }

        return value % 2 == 0 ? value : value - 1;
    }

    AVPixelFormat chooseVideoEncoderPixelFormat(const AVCodec* encoder)
    {
        if (!encoder) {
            return AV_PIX_FMT_YUV420P;
        }

        const std::string encoderName = encoder->name ? encoder->name : "";

        std::vector<AVPixelFormat> preferredFormats;

        if (encoderName == "h264_mf" ||
            encoderName == "hevc_mf" ||
            encoderName == "h264_rkmpp" ||
            encoderName == "hevc_rkmpp") {
            preferredFormats = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
        }
        else {
            preferredFormats = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12 };
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* pixelFormats = getSupportedPixelFormats(encoder, &formatCount);
        if (pixelFormats && formatCount > 0) {
            for (AVPixelFormat preferred : preferredFormats) {
                for (int i = 0; i < formatCount; ++i) {
                    if (pixelFormats[i] == preferred) {
                        return preferred;
                    }
                }
            }

            return pixelFormats[0];
        }
#else
        if (encoder->pix_fmts) {
            for (AVPixelFormat preferred : preferredFormats) {
                for (const AVPixelFormat* p = encoder->pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
                    if (*p == preferred) {
                        return preferred;
                    }
                }
            }

            return encoder->pix_fmts[0];
        }
#endif

        return preferredFormats.front();
    }

} // namespace media::ffmpeg
