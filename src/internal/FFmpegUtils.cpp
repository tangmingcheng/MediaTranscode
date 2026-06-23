#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
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

    bool hasSuffix(const std::string& value, const char* suffix)
    {
        if (!suffix) {
            return false;
        }

        const std::size_t suffixLen = std::strlen(suffix);
        return value.size() >= suffixLen &&
            value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
    }

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

    int audioChannelCount(const AVCodecContext* ctx)
    {
        if (!ctx) {
            return 0;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        return ctx->ch_layout.nb_channels;
#else
        if (ctx->channels > 0) {
            return ctx->channels;
        }

        if (ctx->channel_layout != 0) {
            return av_get_channel_layout_nb_channels(ctx->channel_layout);
        }

        return 0;
#endif
    }

    bool ensureAudioDecoderChannelLayout(AVCodecContext* ctx)
    {
        if (!ctx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        const int channels = ctx->ch_layout.nb_channels;
        if (channels <= 0) {
            return false;
        }

        if (ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            av_channel_layout_uninit(&ctx->ch_layout);
            av_channel_layout_default(&ctx->ch_layout, channels);
        }

        return ctx->ch_layout.nb_channels > 0;
#else
        if (ctx->channel_layout == 0 && ctx->channels > 0) {
            ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
        }

        if (ctx->channels <= 0 && ctx->channel_layout != 0) {
            ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
        }

        return ctx->channels > 0 && ctx->channel_layout != 0;
#endif
    }

    bool copyAudioChannelLayoutToEncoder(AVCodecContext* encoderCtx,
                                         const AVCodecContext* decoderCtx)
    {
        if (!encoderCtx || !decoderCtx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        const int channels = decoderCtx->ch_layout.nb_channels;
        if (channels <= 0) {
            return false;
        }

        av_channel_layout_uninit(&encoderCtx->ch_layout);

        if (decoderCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            AVChannelLayout defaultLayout{};
            av_channel_layout_default(&defaultLayout, channels);
            const int ret = av_channel_layout_copy(&encoderCtx->ch_layout, &defaultLayout);
            av_channel_layout_uninit(&defaultLayout);
            return ret >= 0;
        }

        return av_channel_layout_copy(&encoderCtx->ch_layout, &decoderCtx->ch_layout) >= 0;
#else
        encoderCtx->channels = decoderCtx->channels;
        encoderCtx->channel_layout = decoderCtx->channel_layout;

        if (encoderCtx->channel_layout == 0 && encoderCtx->channels > 0) {
            encoderCtx->channel_layout = av_get_default_channel_layout(encoderCtx->channels);
        }

        if (encoderCtx->channels <= 0 && encoderCtx->channel_layout != 0) {
            encoderCtx->channels = av_get_channel_layout_nb_channels(encoderCtx->channel_layout);
        }

        return encoderCtx->channels > 0 && encoderCtx->channel_layout != 0;
#endif
    }

    bool setFrameAudioLayoutFromCodecContext(AVFrame* frame,
                                             const AVCodecContext* codecCtx)
    {
        if (!frame || !codecCtx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        const int channels = codecCtx->ch_layout.nb_channels;
        if (channels <= 0) {
            return false;
        }

        av_channel_layout_uninit(&frame->ch_layout);

        if (codecCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            AVChannelLayout defaultLayout{};
            av_channel_layout_default(&defaultLayout, channels);
            const int ret = av_channel_layout_copy(&frame->ch_layout, &defaultLayout);
            av_channel_layout_uninit(&defaultLayout);
            return ret >= 0;
        }

        return av_channel_layout_copy(&frame->ch_layout, &codecCtx->ch_layout) >= 0;
#else
        frame->channels = codecCtx->channels;
        frame->channel_layout = codecCtx->channel_layout;

        if (frame->channel_layout == 0 && frame->channels > 0) {
            frame->channel_layout = av_get_default_channel_layout(frame->channels);
        }

        if (frame->channels <= 0 && frame->channel_layout != 0) {
            frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
        }

        return frame->channels > 0 && frame->channel_layout != 0;
#endif
    }

#if LIBAVUTIL_VERSION_MAJOR < 57
    int64_t oldAudioChannelLayout(const AVCodecContext* ctx)
    {
        if (!ctx) {
            return 0;
        }

        if (ctx->channel_layout != 0) {
            return static_cast<int64_t>(ctx->channel_layout);
        }

        if (ctx->channels > 0) {
            return static_cast<int64_t>(av_get_default_channel_layout(ctx->channels));
        }

        return 0;
    }
#endif

    bool isHardwareEncoderName(const char* name)
    {
        if (!name || !*name) {
            return false;
        }

        std::string value(name);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        return hasSuffix(value, "_nvenc") ||
            hasSuffix(value, "_qsv") ||
            hasSuffix(value, "_amf") ||
            hasSuffix(value, "_mf") ||
            hasSuffix(value, "_vaapi") ||
            hasSuffix(value, "_videotoolbox") ||
            hasSuffix(value, "_rkmpp") ||
            hasSuffix(value, "_v4l2m2m") ||
            hasSuffix(value, "_mediacodec") ||
            hasSuffix(value, "_d3d12va");
    }

} // namespace media::ffmpeg
