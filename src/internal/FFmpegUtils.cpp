#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
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

    const AVSampleFormat* getSupportedSampleFormats(const AVCodec* encoder, int* count)
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
            AV_CODEC_CONFIG_SAMPLE_FORMAT,
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

        return static_cast<const AVSampleFormat*>(configs);
    }

    const int* getSupportedSampleRates(const AVCodec* encoder, int* count)
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
            AV_CODEC_CONFIG_SAMPLE_RATE,
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

        return static_cast<const int*>(configs);
    }
#endif

} // namespace

    std::string errorString(int err)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(err, buffer, sizeof(buffer));

        std::ostringstream oss;
        oss << buffer << " (" << err << ")";
        return oss.str();
    }

    const char* preferredAudioEncoderName(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::AAC:
            return "aac";
        case AudioCodec::OPUS:
            return "libopus";
        case AudioCodec::MP3:
            return "libmp3lame";
        case AudioCodec::Auto:
        default:
            return nullptr;
        }
    }

    AVCodecID fallbackAudioCodecId(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::AAC:
            return AV_CODEC_ID_AAC;
        case AudioCodec::OPUS:
            return AV_CODEC_ID_OPUS;
        case AudioCodec::MP3:
            return AV_CODEC_ID_MP3;
        case AudioCodec::Auto:
        default:
            return AV_CODEC_ID_NONE;
        }
    }

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

    AVSampleFormat chooseAudioSampleFormat(const AVCodec* encoder)
    {
        if (!encoder) {
            return AV_SAMPLE_FMT_FLTP;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVSampleFormat* sampleFormats = getSupportedSampleFormats(encoder, &formatCount);
        if (sampleFormats && formatCount > 0) {
            for (int i = 0; i < formatCount; ++i) {
                if (sampleFormats[i] == AV_SAMPLE_FMT_FLTP) {
                    return AV_SAMPLE_FMT_FLTP;
                }
            }

            return sampleFormats[0];
        }
#else
        if (encoder->sample_fmts) {
            for (const AVSampleFormat* p = encoder->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
                if (*p == AV_SAMPLE_FMT_FLTP) {
                    return AV_SAMPLE_FMT_FLTP;
                }
            }

            return encoder->sample_fmts[0];
        }
#endif

        return AV_SAMPLE_FMT_FLTP;
    }

    int chooseAudioSampleRate(const AVCodec* encoder, int preferredRate)
    {
        if (!encoder) {
            return preferredRate > 0 ? preferredRate : 44100;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int rateCount = 0;
        const int* sampleRates = getSupportedSampleRates(encoder, &rateCount);
        if (!sampleRates || rateCount <= 0) {
            return preferredRate > 0 ? preferredRate : 44100;
        }
#else
        const int* sampleRates = encoder->supported_samplerates;
        if (!sampleRates) {
            return preferredRate > 0 ? preferredRate : 44100;
        }
#endif

        if (preferredRate > 0) {
#if LIBAVCODEC_VERSION_MAJOR >= 61
            for (int i = 0; i < rateCount; ++i) {
                if (sampleRates[i] == preferredRate) {
                    return preferredRate;
                }
            }
#else
            for (const int* p = sampleRates; *p; ++p) {
                if (*p == preferredRate) {
                    return preferredRate;
                }
            }
#endif
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        return sampleRates[0];
#else
        return sampleRates[0] > 0 ? sampleRates[0] : 44100;
#endif
    }

} // namespace media::ffmpeg
