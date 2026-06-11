#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {

    std::string errorString(int err)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(err, buffer, sizeof(buffer));

        std::ostringstream oss;
        oss << buffer << " (" << err << ")";
        return oss.str();
    }

    const char* preferredVideoEncoderName(VideoCodec codec)
    {
        switch (codec) {
        case VideoCodec::H264_RKMPP:
            return "h264_rkmpp";
        case VideoCodec::H265_RKMPP:
            return "hevc_rkmpp";
        case VideoCodec::H264_LIBX264:
            return "libx264";
        case VideoCodec::H265_LIBX265:
            return "libx265";
        case VideoCodec::Copy:
        default:
            return nullptr;
        }
    }

    AVCodecID fallbackVideoCodecId(VideoCodec codec)
    {
        switch (codec) {
        case VideoCodec::H264_RKMPP:
        case VideoCodec::H264_LIBX264:
            return AV_CODEC_ID_H264;
        case VideoCodec::H265_RKMPP:
        case VideoCodec::H265_LIBX265:
            return AV_CODEC_ID_HEVC;
        case VideoCodec::Copy:
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

    int chooseOutputFps(const TranscodeConfig& config, const AVStream* inputVideoStream)
    {
        if (config.fps > 0) {
            return config.fps;
        }

        if (inputVideoStream) {
            AVRational rate = inputVideoStream->avg_frame_rate;

            if (rate.num > 0 && rate.den > 0) {
                const double fps = av_q2d(rate);

                if (fps > 1.0 && fps < 240.0) {
                    return static_cast<int>(std::round(fps));
                }
            }

            rate = inputVideoStream->r_frame_rate;

            if (rate.num > 0 && rate.den > 0) {
                const double fps = av_q2d(rate);

                if (fps > 1.0 && fps < 240.0) {
                    return static_cast<int>(std::round(fps));
                }
            }
        }

        return 25;
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
            preferredFormats = {
                AV_PIX_FMT_NV12,
                AV_PIX_FMT_YUV420P
            };
        }
        else {
            preferredFormats = {
                AV_PIX_FMT_YUV420P,
                AV_PIX_FMT_NV12
            };
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61

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

        if (ret < 0) {
            return preferredFormats.front();
        }

        if (!configs) {
            return preferredFormats.front();
        }

        const auto* supportedFormats =
            static_cast<const AVPixelFormat*>(configs);

        auto isSupported = [&](AVPixelFormat fmt) -> bool {
            for (int i = 0; i < configCount; ++i) {
                if (supportedFormats[i] == fmt) {
                    return true;
                }
            }

            return false;
            };

        for (AVPixelFormat preferred : preferredFormats) {
            if (isSupported(preferred)) {
                return preferred;
            }
        }

        if (configCount > 0 && supportedFormats[0] != AV_PIX_FMT_NONE) {
            return supportedFormats[0];
        }

        return AV_PIX_FMT_YUV420P;

#else

        if (!encoder->pix_fmts) {
            return preferredFormats.front();
        }

        auto isSupported = [&](AVPixelFormat fmt) -> bool {
            for (const AVPixelFormat* p = encoder->pix_fmts;
                *p != AV_PIX_FMT_NONE;
                ++p) {
                if (*p == fmt) {
                    return true;
                }
            }

            return false;
            };

        for (AVPixelFormat preferred : preferredFormats) {
            if (isSupported(preferred)) {
                return preferred;
            }
        }

        return encoder->pix_fmts[0];

#endif
    }

    AVSampleFormat chooseAudioSampleFormat(const AVCodec* encoder)
    {
        if (!encoder) {
            return AV_SAMPLE_FMT_FLTP;
        }

        const std::vector<AVSampleFormat> preferredFormats = {
            AV_SAMPLE_FMT_FLTP,
            AV_SAMPLE_FMT_S16P,
            AV_SAMPLE_FMT_S16
        };

#if LIBAVCODEC_VERSION_MAJOR >= 61

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

        if (ret < 0 || !configs) {
            return preferredFormats.front();
        }

        const auto* supportedFormats =
            static_cast<const AVSampleFormat*>(configs);

        auto isSupported = [&](AVSampleFormat fmt) -> bool {
            for (int i = 0; i < configCount; ++i) {
                if (supportedFormats[i] == fmt) {
                    return true;
                }
            }

            return false;
            };

        for (AVSampleFormat preferred : preferredFormats) {
            if (isSupported(preferred)) {
                return preferred;
            }
        }

        if (configCount > 0 && supportedFormats[0] != AV_SAMPLE_FMT_NONE) {
            return supportedFormats[0];
        }

        return AV_SAMPLE_FMT_FLTP;

#else

        if (!encoder->sample_fmts) {
            return preferredFormats.front();
        }

        auto isSupported = [&](AVSampleFormat fmt) -> bool {
            for (const AVSampleFormat* p = encoder->sample_fmts;
                *p != AV_SAMPLE_FMT_NONE;
                ++p) {
                if (*p == fmt) {
                    return true;
                }
            }

            return false;
            };

        for (AVSampleFormat preferred : preferredFormats) {
            if (isSupported(preferred)) {
                return preferred;
            }
        }

        return encoder->sample_fmts[0];

#endif
    }

    int chooseAudioSampleRate(const AVCodec* encoder, int preferredRate)
    {
        const int normalizedPreferredRate = preferredRate > 0 ? preferredRate : 48000;

#if LIBAVCODEC_VERSION_MAJOR >= 61

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
            return normalizedPreferredRate;
        }

        const auto* supportedRates = static_cast<const int*>(configs);

        for (int i = 0; i < configCount; ++i) {
            if (supportedRates[i] == normalizedPreferredRate) {
                return normalizedPreferredRate;
            }
        }

        for (int preferred : { 48000, 44100, 32000 }) {
            for (int i = 0; i < configCount; ++i) {
                if (supportedRates[i] == preferred) {
                    return preferred;
                }
            }
        }

        return supportedRates[0] > 0 ? supportedRates[0] : normalizedPreferredRate;

#else

        if (!encoder || !encoder->supported_samplerates) {
            return normalizedPreferredRate;
        }

        for (const int* p = encoder->supported_samplerates; *p > 0; ++p) {
            if (*p == normalizedPreferredRate) {
                return normalizedPreferredRate;
            }
        }

        for (int preferred : { 48000, 44100, 32000 }) {
            for (const int* p = encoder->supported_samplerates; *p > 0; ++p) {
                if (*p == preferred) {
                    return preferred;
                }
            }
        }

        return encoder->supported_samplerates[0] > 0
            ? encoder->supported_samplerates[0]
            : normalizedPreferredRate;

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
        if (ctx->ch_layout.nb_channels <= 0) {
            av_channel_layout_default(&ctx->ch_layout, 2);
        }

        return ctx->ch_layout.nb_channels > 0;
#else
        if (ctx->channel_layout == 0 && ctx->channels > 0) {
            ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
        }

        if (ctx->channels <= 0 && ctx->channel_layout != 0) {
            ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
        }

        if (ctx->channels <= 0) {
            ctx->channels = 2;
            ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
        }

        return ctx->channel_layout != 0 && ctx->channels > 0;
#endif
    }

    bool copyAudioChannelLayoutToEncoder(AVCodecContext* encoderCtx,
        const AVCodecContext* decoderCtx)
    {
        if (!encoderCtx || !decoderCtx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        if (decoderCtx->ch_layout.nb_channels > 0) {
            return av_channel_layout_copy(
                &encoderCtx->ch_layout,
                &decoderCtx->ch_layout
            ) >= 0;
        }

        av_channel_layout_default(&encoderCtx->ch_layout, 2);
        return encoderCtx->ch_layout.nb_channels > 0;
#else
        encoderCtx->channels = decoderCtx->channels > 0
            ? decoderCtx->channels
            : 2;

        encoderCtx->channel_layout = decoderCtx->channel_layout != 0
            ? decoderCtx->channel_layout
            : av_get_default_channel_layout(encoderCtx->channels);

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
        return av_channel_layout_copy(&frame->ch_layout, &codecCtx->ch_layout) >= 0;
#else
        frame->channel_layout = codecCtx->channel_layout;
        frame->channels = codecCtx->channels;
        return frame->channel_layout != 0 && frame->channels > 0;
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
            return av_get_default_channel_layout(ctx->channels);
        }

        return av_get_default_channel_layout(2);
    }
#endif

    bool isHardwareEncoderName(const char* name)
    {
        if (!name) {
            return false;
        }

        const std::string encoderName(name);

        return encoderName.find("_rkmpp") != std::string::npos ||
            encoderName.find("_mf") != std::string::npos ||
            encoderName.find("_qsv") != std::string::npos ||
            encoderName.find("_nvenc") != std::string::npos ||
            encoderName.find("_amf") != std::string::npos;
    }

    void setVideoEncoderOptions(AVCodecContext* encoderCtx, const AVCodec* encoder)
    {
        if (!encoderCtx || !encoder) {
            return;
        }

        const std::string name = encoder->name ? encoder->name : "";

        encoderCtx->max_b_frames = 0;

        if (name == "libx264") {
            av_opt_set(encoderCtx->priv_data, "preset", "veryfast", 0);
            av_opt_set(encoderCtx->priv_data, "tune", "zerolatency", 0);
        }
        else if (name == "libx265") {
            av_opt_set(encoderCtx->priv_data, "preset", "veryfast", 0);
            av_opt_set(encoderCtx->priv_data, "tune", "zerolatency", 0);
        }
    }

} // namespace media::ffmpeg
