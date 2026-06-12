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
        case VideoCodec::H264:
            return "libx264";
        case VideoCodec::H265:
            return "libx265";
        case VideoCodec::MPEG4:
            return "mpeg4";
        case VideoCodec::VP8:
            return "libvpx";
        case VideoCodec::VP9:
            return "libvpx-vp9";
        case VideoCodec::AV1:
            return "libaom-av1";
        case VideoCodec::Copy:
        default:
            return nullptr;
        }
    }

    AVCodecID fallbackVideoCodecId(VideoCodec codec)
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

    const char* preferredAudioEncoderName(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::AAC:
            return "aac";
        case AudioCodec::OPUS:
            return "libopus";
        case AudioCodec::MP3:
            return "libmp3lame";
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
            preferredFormats = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
        }
        else {
            preferredFormats = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12 };
        }

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

        return preferredFormats.front();
    }

    AVSampleFormat chooseAudioSampleFormat(const AVCodec* encoder)
    {
        if (!encoder || !encoder->sample_fmts) {
            return AV_SAMPLE_FMT_FLTP;
        }

        for (const AVSampleFormat* p = encoder->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
            if (*p == AV_SAMPLE_FMT_FLTP) {
                return AV_SAMPLE_FMT_FLTP;
            }
        }

        return encoder->sample_fmts[0];
    }

    int chooseAudioSampleRate(const AVCodec* encoder, int preferredRate)
    {
        if (!encoder || !encoder->supported_samplerates) {
            return preferredRate > 0 ? preferredRate : 44100;
        }

        for (const int* p = encoder->supported_samplerates; *p; ++p) {
            if (*p == preferredRate) {
                return preferredRate;
            }
        }

        int bestRate = encoder->supported_samplerates[0];
        int bestDiff = std::abs(bestRate - preferredRate);

        for (const int* p = encoder->supported_samplerates; *p; ++p) {
            const int diff = std::abs(*p - preferredRate);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestRate = *p;
            }
        }

        return bestRate > 0 ? bestRate : 44100;
    }

    int audioChannelCount(const AVCodecContext* ctx)
    {
        if (!ctx) {
            return 0;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        return ctx->ch_layout.nb_channels;
#else
        return ctx->channels;
#endif
    }

    bool ensureAudioDecoderChannelLayout(AVCodecContext* ctx)
    {
        if (!ctx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        return ctx->ch_layout.nb_channels > 0;
#else
        if (ctx->channel_layout != 0) {
            return true;
        }

        if (ctx->channels > 0) {
            ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
            return ctx->channel_layout != 0;
        }

        return false;
#endif
    }

    bool copyAudioChannelLayoutToEncoder(AVCodecContext* encoderCtx,
                                         const AVCodecContext* decoderCtx)
    {
        if (!encoderCtx || !decoderCtx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        if (decoderCtx->ch_layout.nb_channels <= 0) {
            return false;
        }

        av_channel_layout_uninit(&encoderCtx->ch_layout);
        return av_channel_layout_copy(&encoderCtx->ch_layout, &decoderCtx->ch_layout) >= 0;
#else
        if (decoderCtx->channel_layout == 0 || decoderCtx->channels <= 0) {
            return false;
        }

        encoderCtx->channel_layout = decoderCtx->channel_layout;
        encoderCtx->channels = decoderCtx->channels;
        return true;
#endif
    }

    bool setFrameAudioLayoutFromCodecContext(AVFrame* frame,
                                             const AVCodecContext* codecCtx)
    {
        if (!frame || !codecCtx) {
            return false;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        if (codecCtx->ch_layout.nb_channels <= 0) {
            return false;
        }

        av_channel_layout_uninit(&frame->ch_layout);
        return av_channel_layout_copy(&frame->ch_layout, &codecCtx->ch_layout) >= 0;
#else
        if (codecCtx->channel_layout == 0 || codecCtx->channels <= 0) {
            return false;
        }

        frame->channel_layout = codecCtx->channel_layout;
        frame->channels = codecCtx->channels;
        return true;
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

        return 0;
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
            encoderName.find("_vaapi") != std::string::npos ||
            encoderName.find("_amf") != std::string::npos;
    }

    void setVideoEncoderOptions(AVCodecContext* encoderCtx, const AVCodec* encoder)
    {
        if (!encoderCtx || !encoder) {
            return;
        }

        const std::string encoderName = encoder->name ? encoder->name : "";

        if (encoderName == "libx264") {
            av_opt_set(encoderCtx->priv_data, "preset", "veryfast", 0);
            av_opt_set(encoderCtx->priv_data, "tune", "zerolatency", 0);
        }
        else if (encoderName == "libx265") {
            av_opt_set(encoderCtx->priv_data, "preset", "veryfast", 0);
            av_opt_set(encoderCtx->priv_data, "tune", "zerolatency", 0);
        }
        else if (encoderName == "libvpx" || encoderName == "libvpx-vp9") {
            av_opt_set(encoderCtx->priv_data, "deadline", "realtime", 0);
            av_opt_set(encoderCtx->priv_data, "cpu-used", "4", 0);
        }
        else if (encoderName == "libaom-av1") {
            av_opt_set(encoderCtx->priv_data, "cpu-used", "6", 0);
            av_opt_set(encoderCtx->priv_data, "row-mt", "1", 0);
        }
    }

} // namespace media::ffmpeg
