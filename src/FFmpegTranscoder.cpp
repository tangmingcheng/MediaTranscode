#include "media_transcode/FFmpegTranscoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/version_major.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace media {

    namespace {

        std::string ffErrorString(int err)
        {
            char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
            av_strerror(err, buffer, sizeof(buffer));

            std::ostringstream oss;
            oss << buffer << " (" << err << ")";
            return oss.str();
        }

        const char* preferredEncoderName(VideoCodec codec)
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

        AVCodecID fallbackCodecId(VideoCodec codec)
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

            /*
             * H.264/H.265 + yuv420p/nv12 通常要求宽高为偶数。
             */
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

        AVPixelFormat chooseEncoderPixelFormat(const AVCodec* encoder)
        {
            if (!encoder) {
                return AV_PIX_FMT_YUV420P;
            }

            const std::string encoderName = encoder->name ? encoder->name : "";

            /*
             * 不同编码器优先选择不同像素格式：
             *
             * - libx264 / libx265：优先 yuv420p，兼容性最好。
             * - h264_mf / hevc_mf：Windows MediaFoundation 通常更适合 NV12。
             * - h264_rkmpp / hevc_rkmpp：RKMPP 通常也优先 NV12。
             */
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

            /*
             * FFmpeg 文档说明：
             * out_configs 为 NULL 表示该 codec 支持所有可能值。
             */
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

            /*
             * 兼容旧 FFmpeg。
             * 旧版本没有 avcodec_get_supported_config，只能使用 pix_fmts。
             */
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

        void setEncoderOptions(AVCodecContext* encoderCtx, const AVCodec* encoder)
        {
            if (!encoderCtx || !encoder) {
                return;
            }

            const std::string name = encoder->name ? encoder->name : "";

            /*
             * 第一版禁用 B 帧，减少 DTS/PTS 乱序复杂度。
             * 对 MP4 封装和实时转码更稳定。
             */
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

        bool checkRet(int ret, std::string* error, const std::string& prefix)
        {
            if (ret >= 0) {
                return true;
            }

            if (error) {
                *error = prefix + ": " + ffErrorString(ret);
            }

            return false;
        }

    } // namespace

    FFmpegTranscoder::FFmpegTranscoder()
    {
        avformat_network_init();
    }

    FFmpegTranscoder::~FFmpegTranscoder()
    {
        stop();
    }

    bool FFmpegTranscoder::initialize(const TranscodeConfig& config)
    {
        if (m_running.load()) {
            setLastError("initialize failed: transcoder is running");
            return false;
        }

        if (config.inputUrl.empty()) {
            setLastError("initialize failed: inputUrl is empty");
            return false;
        }

        if (config.outputUrl.empty()) {
            setLastError("initialize failed: outputUrl is empty");
            return false;
        }

        if (config.videoCodec == VideoCodec::Copy) {
            setLastError("initialize failed: VideoCodec::Copy is not implemented in first transcoding version");
            return false;
        }

        m_config = config;
        clearLastError();

        return true;
    }

    bool FFmpegTranscoder::start()
    {
        if (m_running.load()) {
            setLastError("start failed: transcoder is already running");
            return false;
        }

        if (m_config.inputUrl.empty() || m_config.outputUrl.empty()) {
            setLastError("start failed: transcoder is not initialized");
            return false;
        }

        /*
         * 防止同一个对象二次 start 时，之前线程虽然结束但还没有 join。
         */
        if (m_transcodeThread.joinable()) {
            m_transcodeThread.join();
        }

        clearLastError();

        m_stopRequested.store(false);
        m_running.store(true);

        try {
            m_transcodeThread = std::thread(&FFmpegTranscoder::transcodeThread, this);
        }
        catch (const std::exception& e) {
            m_running.store(false);
            setLastError(std::string("start failed: create thread failed: ") + e.what());
            return false;
        }

        return true;
    }

    void FFmpegTranscoder::stop()
    {
        m_stopRequested.store(true);

        if (m_transcodeThread.joinable()) {
            m_transcodeThread.join();
        }

        m_running.store(false);
    }

    bool FFmpegTranscoder::wait()
    {
        if (m_transcodeThread.joinable()) {
            m_transcodeThread.join();
        }

        return !m_running.load();
    }

    bool FFmpegTranscoder::isRunning() const
    {
        return m_running.load();
    }

    std::string FFmpegTranscoder::lastError() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastError;
    }

    bool FFmpegTranscoder::pushFrame(void* frame)
    {
        (void)frame;
        setLastError("pushFrame is not supported in url/file transcoder version");
        return false;
    }

    void FFmpegTranscoder::setProgressCallback(ProgressCallback cb)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progressCallback = std::move(cb);
    }

    void FFmpegTranscoder::transcodeThread()
    {
        AVFormatContext* inputFmtCtx = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;

        AVCodecContext* decoderCtx = nullptr;
        AVCodecContext* encoderCtx = nullptr;

        AVCodecContext* audioDecoderCtx = nullptr;
        AVCodecContext* audioEncoderCtx = nullptr;
        SwrContext* swrCtx = nullptr;
        AVAudioFifo* audioFifo = nullptr;

        AVFilterGraph* videoFilterGraph = nullptr;
        AVFilterContext* videoBufferSrcCtx = nullptr;
        AVFilterContext* videoBufferSinkCtx = nullptr;

        AVPacket* inputPacket = nullptr;
        AVFrame* decodedFrame = nullptr;
        AVFrame* filteredFrame = nullptr;
        AVFrame* decodedAudioFrame = nullptr;

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

        AVStream* inputVideoStream = nullptr;
        AVStream* inputAudioStream = nullptr;

        AVStream* outputVideoStream = nullptr;
        AVStream* outputAudioStream = nullptr;

        int64_t encodedVideoPacketCount = 0;
        int64_t encodedAudioPacketCount = 0;

        int64_t timelineStartUs = AV_NOPTS_VALUE;
        int64_t lastSubmittedVideoPts = AV_NOPTS_VALUE;
        int64_t lastWrittenVideoDts = AV_NOPTS_VALUE;
        int64_t lastWrittenAudioDts = AV_NOPTS_VALUE;
        int64_t lastWrittenVideoOutTimeMs = 0;
        int64_t lastWrittenAudioOutTimeMs = 0;

        int64_t nextAudioPts = AV_NOPTS_VALUE;

        int outputFps = 0;
        int outputWidth = 0;
        int outputHeight = 0;
        bool enableConstantFps = false;

        AVRational filterInputFrameRate{ 0, 1 };

        const auto startTime = std::chrono::steady_clock::now();

        auto emitProgress = [&](const std::string& raw) {
            ProgressCallback callback;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                callback = m_progressCallback;
            }

            if (!callback) {
                return;
            }

            ProgressInfo info;
            info.frame = encodedVideoPacketCount;

            info.outTimeMs = std::max(lastWrittenVideoOutTimeMs, lastWrittenAudioOutTimeMs);

            if (info.outTimeMs <= 0 && encoderCtx && encoderCtx->framerate.num > 0) {
                info.outTimeMs = static_cast<int64_t>(encodedVideoPacketCount * 1000.0 *
                    encoderCtx->framerate.den /
                    encoderCtx->framerate.num);
            }

            const auto now = std::chrono::steady_clock::now();
            const double elapsedSec =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;

            if (elapsedSec > 0.001) {
                const double mediaSec = info.outTimeMs / 1000.0;
                info.speed = mediaSec / elapsedSec;
            }
            else {
                info.speed = 0.0;
            }

            info.raw = raw;

            callback(info);
            };

        auto toUs = [](int64_t timestamp, AVRational timeBase) -> int64_t {
            if (timestamp == AV_NOPTS_VALUE) {
                return AV_NOPTS_VALUE;
            }

            return av_rescale_q(timestamp, timeBase, AVRational{ 1, AV_TIME_BASE });
            };

        auto fromUs = [](int64_t timestampUs, AVRational timeBase) -> int64_t {
            if (timestampUs == AV_NOPTS_VALUE) {
                return AV_NOPTS_VALUE;
            }

            return av_rescale_q(timestampUs, AVRational{ 1, AV_TIME_BASE }, timeBase);
            };

        auto initTimelineStart = [&](int64_t timestampUs) {
            if (timestampUs == AV_NOPTS_VALUE) {
                return;
            }

            if (timelineStartUs == AV_NOPTS_VALUE) {
                timelineStartUs = timestampUs;
            }
            };

        auto normalizeUs = [&](int64_t timestampUs) -> int64_t {
            if (timestampUs == AV_NOPTS_VALUE) {
                return AV_NOPTS_VALUE;
            }

            initTimelineStart(timestampUs);

            if (timelineStartUs == AV_NOPTS_VALUE) {
                return AV_NOPTS_VALUE;
            }

            const int64_t normalized = timestampUs - timelineStartUs;

            /*
             * 输出 MP4 不建议保留负时间戳。
             * 这里不是强制同步，而是统一将媒体起点归零。
             */
            return std::max<int64_t>(0, normalized);
            };

        auto initTimelineStartFromFormat = [&]() {
            if (inputFmtCtx && inputFmtCtx->start_time != AV_NOPTS_VALUE) {
                initTimelineStart(inputFmtCtx->start_time);
            }

            if (inputVideoStream && inputVideoStream->start_time != AV_NOPTS_VALUE) {
                initTimelineStart(toUs(inputVideoStream->start_time, inputVideoStream->time_base));
            }

            if (inputAudioStream && inputAudioStream->start_time != AV_NOPTS_VALUE) {
                initTimelineStart(toUs(inputAudioStream->start_time, inputAudioStream->time_base));
            }
            };

        auto fail = [&](const std::string& error) {
            setLastError(error);
            };

        auto cleanup = [&]() {
            if (audioFifo) {
                av_audio_fifo_free(audioFifo);
                audioFifo = nullptr;
            }

            if (swrCtx) {
                swr_free(&swrCtx);
            }

            if (decodedAudioFrame) {
                av_frame_free(&decodedAudioFrame);
            }

            if (filteredFrame) {
                av_frame_free(&filteredFrame);
            }

            if (decodedFrame) {
                av_frame_free(&decodedFrame);
            }

            if (inputPacket) {
                av_packet_free(&inputPacket);
            }

            if (videoFilterGraph) {
                avfilter_graph_free(&videoFilterGraph);
                videoFilterGraph = nullptr;
                videoBufferSrcCtx = nullptr;
                videoBufferSinkCtx = nullptr;
            }

            if (audioDecoderCtx) {
                avcodec_free_context(&audioDecoderCtx);
            }

            if (audioEncoderCtx) {
                avcodec_free_context(&audioEncoderCtx);
            }

            if (decoderCtx) {
                avcodec_free_context(&decoderCtx);
            }

            if (encoderCtx) {
                avcodec_free_context(&encoderCtx);
            }

            if (inputFmtCtx) {
                avformat_close_input(&inputFmtCtx);
            }

            if (outputFmtCtx) {
                if (!(outputFmtCtx->oformat->flags & AVFMT_NOFILE) && outputFmtCtx->pb) {
                    avio_closep(&outputFmtCtx->pb);
                }

                avformat_free_context(outputFmtCtx);
                outputFmtCtx = nullptr;
            }

            m_running.store(false);
            };

        emitProgress("initialized");

        int ret = 0;

        /*
         * 1. 打开输入
         */
        ret = avformat_open_input(&inputFmtCtx, m_config.inputUrl.c_str(), nullptr, nullptr);
        if (ret < 0) {
            fail("avformat_open_input failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        ret = avformat_find_stream_info(inputFmtCtx, nullptr);
        if (ret < 0) {
            fail("avformat_find_stream_info failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        /*
         * 2. 查找视频流 / 音频流
         */
        ret = av_find_best_stream(inputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (ret < 0) {
            fail("av_find_best_stream video failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        videoStreamIndex = ret;
        inputVideoStream = inputFmtCtx->streams[videoStreamIndex];

        if (m_config.audioMode != AudioMode::None) {
            ret = av_find_best_stream(inputFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (ret >= 0) {
                audioStreamIndex = ret;
                inputAudioStream = inputFmtCtx->streams[audioStreamIndex];
            }
        }

        initTimelineStartFromFormat();

        /*
         * 3. 打开视频解码器
         */
        {
            const AVCodec* decoder = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
            if (!decoder) {
                fail("avcodec_find_decoder failed: unsupported input video codec");
                cleanup();
                return;
            }

            decoderCtx = avcodec_alloc_context3(decoder);
            if (!decoderCtx) {
                fail("avcodec_alloc_context3 decoder failed");
                cleanup();
                return;
            }

            ret = avcodec_parameters_to_context(decoderCtx, inputVideoStream->codecpar);
            if (ret < 0) {
                fail("avcodec_parameters_to_context decoder failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            ret = avcodec_open2(decoderCtx, decoder, nullptr);
            if (ret < 0) {
                fail("avcodec_open2 decoder failed: " + ffErrorString(ret));
                cleanup();
                return;
            }
        }

        /*
         * 4. 创建输出上下文
         */
        ret = avformat_alloc_output_context2(&outputFmtCtx, nullptr, nullptr, m_config.outputUrl.c_str());
        if (ret < 0 || !outputFmtCtx) {
            fail("avformat_alloc_output_context2 failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        /*
         * 5. 创建并打开视频编码器
         */
        {
            const char* encoderName = preferredEncoderName(m_config.videoCodec);
            const AVCodec* encoder = nullptr;

            if (encoderName) {
                encoder = avcodec_find_encoder_by_name(encoderName);
            }

            if (!encoder) {
                const AVCodecID codecId = fallbackCodecId(m_config.videoCodec);
                encoder = avcodec_find_encoder(codecId);
            }

            if (!encoder) {
                fail("avcodec_find_encoder failed: no suitable video encoder");
                cleanup();
                return;
            }

            encoderCtx = avcodec_alloc_context3(encoder);
            if (!encoderCtx) {
                fail("avcodec_alloc_context3 encoder failed");
                cleanup();
                return;
            }

            outputFps = chooseOutputFps(m_config, inputVideoStream);
            enableConstantFps = m_config.fps > 0;

            outputWidth = m_config.width > 0 ? m_config.width : decoderCtx->width;
            outputHeight = m_config.height > 0 ? m_config.height : decoderCtx->height;

            outputWidth = normalizeEvenSize(outputWidth);
            outputHeight = normalizeEvenSize(outputHeight);

            if (outputWidth <= 0 || outputHeight <= 0) {
                fail("invalid output size");
                cleanup();
                return;
            }

            encoderCtx->width = outputWidth;
            encoderCtx->height = outputHeight;

            /*
             * fps 语义：
             *
             * m_config.fps > 0:
             *     真正做固定帧率转换，编码器时间基使用 1/fps。
             *
             * m_config.fps <= 0:
             *     保留输入时间轴，编码器时间基优先沿用输入视频流 time_base。
             */
            AVRational encoderTimeBase = AVRational{ 1, outputFps };

            if (!enableConstantFps) {
                encoderTimeBase = inputVideoStream->time_base;

                if (encoderTimeBase.num <= 0 || encoderTimeBase.den <= 0) {
                    encoderTimeBase = AVRational{ 1, outputFps };
                }
            }

            encoderCtx->time_base = encoderTimeBase;
            encoderCtx->framerate = AVRational{ outputFps, 1 };
            encoderCtx->pix_fmt = chooseEncoderPixelFormat(encoder);

            encoderCtx->bit_rate = static_cast<int64_t>(std::max(1, m_config.videoBitrateKbps)) * 1000;
            encoderCtx->gop_size = std::max(10, outputFps * 2);
            encoderCtx->max_b_frames = 0;

            if (outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            setEncoderOptions(encoderCtx, encoder);

            ret = avcodec_open2(encoderCtx, encoder, nullptr);
            if (ret < 0) {
                fail(std::string("avcodec_open2 encoder failed [") +
                    (encoder->name ? encoder->name : "unknown") + "]: " +
                    ffErrorString(ret));
                cleanup();
                return;
            }

            outputVideoStream = avformat_new_stream(outputFmtCtx, nullptr);
            if (!outputVideoStream) {
                fail("avformat_new_stream video failed");
                cleanup();
                return;
            }

            outputVideoStream->time_base = encoderCtx->time_base;

            ret = avcodec_parameters_from_context(outputVideoStream->codecpar, encoderCtx);
            if (ret < 0) {
                fail("avcodec_parameters_from_context video failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            outputVideoStream->codecpar->codec_tag = 0;
        }

        /*
         * 6. 音频处理：
         *
         * - None：不输出音频。
         * - CopySelected：直接复制输入音频 packet。
         * - EncodeSelected：解码输入音频，重采样后统一编码为 AAC。
         */
        if (inputAudioStream && m_config.audioMode == AudioMode::CopySelected) {
            outputAudioStream = avformat_new_stream(outputFmtCtx, nullptr);
            if (!outputAudioStream) {
                fail("avformat_new_stream audio failed");
                cleanup();
                return;
            }

            ret = avcodec_parameters_copy(outputAudioStream->codecpar, inputAudioStream->codecpar);
            if (ret < 0) {
                fail("avcodec_parameters_copy audio failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            outputAudioStream->codecpar->codec_tag = 0;
            outputAudioStream->time_base = inputAudioStream->time_base;
        }
        else if (inputAudioStream && m_config.audioMode == AudioMode::EncodeSelected) {
            const AVCodec* audioDecoder =
                avcodec_find_decoder(inputAudioStream->codecpar->codec_id);

            if (!audioDecoder) {
                fail("avcodec_find_decoder audio failed: unsupported input audio codec");
                cleanup();
                return;
            }

            audioDecoderCtx = avcodec_alloc_context3(audioDecoder);
            if (!audioDecoderCtx) {
                fail("avcodec_alloc_context3 audio decoder failed");
                cleanup();
                return;
            }

            ret = avcodec_parameters_to_context(audioDecoderCtx, inputAudioStream->codecpar);
            if (ret < 0) {
                fail("avcodec_parameters_to_context audio decoder failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            audioDecoderCtx->pkt_timebase = inputAudioStream->time_base;

            ret = avcodec_open2(audioDecoderCtx, audioDecoder, nullptr);
            if (ret < 0) {
                fail("avcodec_open2 audio decoder failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            if (!ensureAudioDecoderChannelLayout(audioDecoderCtx)) {
                fail("invalid input audio channel layout");
                cleanup();
                return;
            }

            const AVCodec* audioEncoder = avcodec_find_encoder_by_name("aac");
            if (!audioEncoder) {
                audioEncoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
            }

            if (!audioEncoder) {
                fail("avcodec_find_encoder audio failed: AAC encoder not found");
                cleanup();
                return;
            }

            audioEncoderCtx = avcodec_alloc_context3(audioEncoder);
            if (!audioEncoderCtx) {
                fail("avcodec_alloc_context3 audio encoder failed");
                cleanup();
                return;
            }

            if (!copyAudioChannelLayoutToEncoder(audioEncoderCtx, audioDecoderCtx)) {
                fail("copy audio channel layout to encoder failed");
                cleanup();
                return;
            }

            audioEncoderCtx->sample_rate =
                chooseAudioSampleRate(audioEncoder, audioDecoderCtx->sample_rate);
            audioEncoderCtx->sample_fmt = chooseAudioSampleFormat(audioEncoder);
            audioEncoderCtx->time_base = AVRational{ 1, audioEncoderCtx->sample_rate };
            audioEncoderCtx->bit_rate =
                static_cast<int64_t>(std::max(32, m_config.audioBitrateKbps)) * 1000;

            if (outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                audioEncoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            ret = avcodec_open2(audioEncoderCtx, audioEncoder, nullptr);
            if (ret < 0) {
                fail(std::string("avcodec_open2 audio encoder failed [") +
                    (audioEncoder->name ? audioEncoder->name : "unknown") + "]: " +
                    ffErrorString(ret));
                cleanup();
                return;
            }

            outputAudioStream = avformat_new_stream(outputFmtCtx, nullptr);
            if (!outputAudioStream) {
                fail("avformat_new_stream encoded audio failed");
                cleanup();
                return;
            }

            outputAudioStream->time_base = audioEncoderCtx->time_base;

            ret = avcodec_parameters_from_context(outputAudioStream->codecpar, audioEncoderCtx);
            if (ret < 0) {
                fail("avcodec_parameters_from_context audio failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            outputAudioStream->codecpar->codec_tag = 0;

#if LIBAVUTIL_VERSION_MAJOR >= 57
            ret = swr_alloc_set_opts2(
                &swrCtx,
                &audioEncoderCtx->ch_layout,
                audioEncoderCtx->sample_fmt,
                audioEncoderCtx->sample_rate,
                &audioDecoderCtx->ch_layout,
                audioDecoderCtx->sample_fmt,
                audioDecoderCtx->sample_rate,
                0,
                nullptr
            );

            if (ret < 0 || !swrCtx) {
                fail("swr_alloc_set_opts2 failed: " + ffErrorString(ret));
                cleanup();
                return;
            }
#else
            swrCtx = swr_alloc_set_opts(
                nullptr,
                oldAudioChannelLayout(audioEncoderCtx),
                audioEncoderCtx->sample_fmt,
                audioEncoderCtx->sample_rate,
                oldAudioChannelLayout(audioDecoderCtx),
                audioDecoderCtx->sample_fmt,
                audioDecoderCtx->sample_rate,
                0,
                nullptr
            );

            if (!swrCtx) {
                fail("swr_alloc_set_opts failed");
                cleanup();
                return;
            }
#endif

            ret = swr_init(swrCtx);
            if (ret < 0) {
                fail("swr_init failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            const int outputChannels = audioChannelCount(audioEncoderCtx);
            if (outputChannels <= 0) {
                fail("invalid output audio channel count");
                cleanup();
                return;
            }

            audioFifo = av_audio_fifo_alloc(
                audioEncoderCtx->sample_fmt,
                outputChannels,
                audioEncoderCtx->frame_size > 0 ? audioEncoderCtx->frame_size : 1024
            );

            if (!audioFifo) {
                fail("av_audio_fifo_alloc failed");
                cleanup();
                return;
            }
        }

        /*
         * 7. 打开输出文件并写 header
         */
        if (!(outputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outputFmtCtx->pb, m_config.outputUrl.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                fail("avio_open output failed: " + ffErrorString(ret));
                cleanup();
                return;
            }
        }

        ret = avformat_write_header(outputFmtCtx, nullptr);
        if (ret < 0) {
            fail("avformat_write_header failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        /*
         * 8. 分配 packet / frame
         */
        inputPacket = av_packet_alloc();
        decodedFrame = av_frame_alloc();
        filteredFrame = av_frame_alloc();

        if (audioDecoderCtx) {
            decodedAudioFrame = av_frame_alloc();
        }

        if (!inputPacket || !decodedFrame || !filteredFrame ||
            (audioDecoderCtx && !decodedAudioFrame)) {
            fail("av_packet_alloc / av_frame_alloc failed");
            cleanup();
            return;
        }

        auto chooseInputFrameRate = [&]() -> AVRational {
            if (inputVideoStream->avg_frame_rate.num > 0 &&
                inputVideoStream->avg_frame_rate.den > 0) {
                return inputVideoStream->avg_frame_rate;
            }

            if (inputVideoStream->r_frame_rate.num > 0 &&
                inputVideoStream->r_frame_rate.den > 0) {
                return inputVideoStream->r_frame_rate;
            }

            return AVRational{ outputFps, 1 };
            };

        auto buildVideoFilterDescription = [&]() -> std::string {
            const char* pixFmtName = av_get_pix_fmt_name(encoderCtx->pix_fmt);
            if (!pixFmtName) {
                return {};
            }

            std::ostringstream desc;

            /*
             * 规范视频处理链：
             *
             * 1. scale 负责尺寸转换。
             * 2. fps 只在 config.fps > 0 时启用，负责真正丢帧/补帧。
             * 3. format 负责输出编码器需要的像素格式。
             */
            desc << "scale="
                << encoderCtx->width
                << ":"
                << encoderCtx->height
                << ":flags=bicubic";

            if (enableConstantFps) {
                desc << ",fps=fps=" << outputFps << ":round=near";
            }

            desc << ",format=pix_fmts=" << pixFmtName;

            return desc.str();
            };

        auto initVideoFilterGraph = [&]() -> bool {
            const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
            const AVFilter* bufferSink = avfilter_get_by_name("buffersink");

            if (!bufferSrc || !bufferSink) {
                fail("avfilter_get_by_name buffer/buffersink failed");
                return false;
            }

            videoFilterGraph = avfilter_graph_alloc();
            if (!videoFilterGraph) {
                fail("avfilter_graph_alloc failed");
                return false;
            }

            filterInputFrameRate = chooseInputFrameRate();

            const AVRational pixelAspect =
                decoderCtx->sample_aspect_ratio.num > 0 && decoderCtx->sample_aspect_ratio.den > 0
                ? decoderCtx->sample_aspect_ratio
                : AVRational{ 1, 1 };

            char args[512] = {};
            std::snprintf(
                args,
                sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                decoderCtx->width,
                decoderCtx->height,
                decoderCtx->pix_fmt,
                inputVideoStream->time_base.num,
                inputVideoStream->time_base.den,
                pixelAspect.num,
                pixelAspect.den,
                filterInputFrameRate.num,
                filterInputFrameRate.den
            );

            ret = avfilter_graph_create_filter(
                &videoBufferSrcCtx,
                bufferSrc,
                "in",
                args,
                nullptr,
                videoFilterGraph
            );

            if (ret < 0) {
                fail("avfilter_graph_create_filter buffer failed: " + ffErrorString(ret));
                return false;
            }

            ret = avfilter_graph_create_filter(
                &videoBufferSinkCtx,
                bufferSink,
                "out",
                nullptr,
                nullptr,
                videoFilterGraph
            );

            if (ret < 0) {
                fail("avfilter_graph_create_filter buffersink failed: " + ffErrorString(ret));
                return false;
            }

            const std::string filterDesc = buildVideoFilterDescription();
            if (filterDesc.empty()) {
                fail("buildVideoFilterDescription failed: invalid encoder pixel format");
                return false;
            }

            AVFilterInOut* outputs = avfilter_inout_alloc();
            AVFilterInOut* inputs = avfilter_inout_alloc();

            if (!outputs || !inputs) {
                avfilter_inout_free(&outputs);
                avfilter_inout_free(&inputs);
                fail("avfilter_inout_alloc failed");
                return false;
            }

            outputs->name = av_strdup("in");
            outputs->filter_ctx = videoBufferSrcCtx;
            outputs->pad_idx = 0;
            outputs->next = nullptr;

            inputs->name = av_strdup("out");
            inputs->filter_ctx = videoBufferSinkCtx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            ret = avfilter_graph_parse_ptr(
                videoFilterGraph,
                filterDesc.c_str(),
                &inputs,
                &outputs,
                nullptr
            );

            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);

            if (ret < 0) {
                fail("avfilter_graph_parse_ptr failed [" + filterDesc + "]: " + ffErrorString(ret));
                return false;
            }

            ret = avfilter_graph_config(videoFilterGraph, nullptr);
            if (ret < 0) {
                fail("avfilter_graph_config failed [" + filterDesc + "]: " + ffErrorString(ret));
                return false;
            }

            return true;
            };

        if (!initVideoFilterGraph()) {
            cleanup();
            return;
        }

        auto writeEncodedVideoPackets = [&](AVFrame* frame) -> bool {
            int sendRet = avcodec_send_frame(encoderCtx, frame);
            if (sendRet < 0) {
                fail("avcodec_send_frame encoder failed: " + ffErrorString(sendRet));
                return false;
            }

            while (true) {
                AVPacket* encodedPacket = av_packet_alloc();
                if (!encodedPacket) {
                    fail("av_packet_alloc encodedPacket failed");
                    return false;
                }

                int receiveRet = avcodec_receive_packet(encoderCtx, encodedPacket);

                if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
                    av_packet_free(&encodedPacket);
                    break;
                }

                if (receiveRet < 0) {
                    const std::string error = "avcodec_receive_packet encoder failed: " +
                        ffErrorString(receiveRet);
                    av_packet_free(&encodedPacket);
                    fail(error);
                    return false;
                }

                encodedPacket->stream_index = outputVideoStream->index;

                if (encodedPacket->duration <= 0) {
                    encodedPacket->duration = 1;
                }

                /*
                 * 关键点：
                 * 编码器 packet 的时间基是 encoderCtx->time_base；
                 * 写 muxer 前必须转换成 outputVideoStream->time_base。
                 */
                av_packet_rescale_ts(
                    encodedPacket,
                    encoderCtx->time_base,
                    outputVideoStream->time_base
                );

                if (encodedPacket->dts != AV_NOPTS_VALUE) {
                    if (lastWrittenVideoDts != AV_NOPTS_VALUE &&
                        encodedPacket->dts <= lastWrittenVideoDts) {
                        std::ostringstream oss;
                        oss << "encoded video packet dts is not strictly increasing: current="
                            << encodedPacket->dts
                            << ", last="
                            << lastWrittenVideoDts;

                        av_packet_free(&encodedPacket);
                        fail(oss.str());
                        return false;
                    }

                    lastWrittenVideoDts = encodedPacket->dts;
                }

                if (encodedPacket->pts != AV_NOPTS_VALUE &&
                    encodedPacket->dts != AV_NOPTS_VALUE &&
                    encodedPacket->pts < encodedPacket->dts) {
                    std::ostringstream oss;
                    oss << "encoded video packet pts is smaller than dts: pts="
                        << encodedPacket->pts
                        << ", dts="
                        << encodedPacket->dts;

                    av_packet_free(&encodedPacket);
                    fail(oss.str());
                    return false;
                }

                if (encodedPacket->duration <= 0) {
                    encodedPacket->duration = 1;
                }

                const int64_t progressTs = encodedPacket->pts != AV_NOPTS_VALUE
                    ? encodedPacket->pts
                    : encodedPacket->dts;

                if (progressTs != AV_NOPTS_VALUE) {
                    lastWrittenVideoOutTimeMs =
                        std::max<int64_t>(
                            lastWrittenVideoOutTimeMs,
                            toUs(progressTs, outputVideoStream->time_base) / 1000
                        );
                }

                ret = av_interleaved_write_frame(outputFmtCtx, encodedPacket);

                av_packet_free(&encodedPacket);

                if (ret < 0) {
                    fail("av_interleaved_write_frame video failed: " + ffErrorString(ret));
                    return false;
                }

                ++encodedVideoPacketCount;

                if (encodedVideoPacketCount == 1 || encodedVideoPacketCount % 25 == 0) {
                    emitProgress("transcoding");
                }
            }

            return true;
            };

        auto drainVideoFilterGraph = [&]() -> bool {
            while (true) {
                ret = av_buffersink_get_frame(videoBufferSinkCtx, filteredFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    return true;
                }

                if (ret < 0) {
                    fail("av_buffersink_get_frame failed: " + ffErrorString(ret));
                    return false;
                }

                const AVRational filterTimeBase = av_buffersink_get_time_base(videoBufferSinkCtx);

                if (filteredFrame->pts == AV_NOPTS_VALUE) {
                    av_frame_unref(filteredFrame);
                    fail("filtered video frame has invalid pts");
                    return false;
                }

                filteredFrame->pts = av_rescale_q(
                    filteredFrame->pts,
                    filterTimeBase,
                    encoderCtx->time_base
                );

                if (lastSubmittedVideoPts != AV_NOPTS_VALUE &&
                    filteredFrame->pts <= lastSubmittedVideoPts) {
                    std::ostringstream oss;
                    oss << "filtered video timestamp is not strictly increasing: current="
                        << filteredFrame->pts
                        << ", last="
                        << lastSubmittedVideoPts;

                    av_frame_unref(filteredFrame);
                    fail(oss.str());
                    return false;
                }

                lastSubmittedVideoPts = filteredFrame->pts;

                const bool ok = writeEncodedVideoPackets(filteredFrame);

                av_frame_unref(filteredFrame);

                if (!ok) {
                    return false;
                }
            }
            };

        auto getDecodedVideoTimestamp = [&]() -> int64_t {
            if (decodedFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
                return decodedFrame->best_effort_timestamp;
            }

            if (decodedFrame->pts != AV_NOPTS_VALUE) {
                return decodedFrame->pts;
            }

            if (decodedFrame->pkt_dts != AV_NOPTS_VALUE) {
                return decodedFrame->pkt_dts;
            }

            return AV_NOPTS_VALUE;
            };

        auto processDecodedFrame = [&]() -> bool {
            const int64_t inputVideoTs = getDecodedVideoTimestamp();

            if (inputVideoTs == AV_NOPTS_VALUE) {
                fail("input video frame has no valid timestamp; refuse to synthesize PTS in normalized transcoder");
                return false;
            }

            const int64_t inputVideoUs = toUs(inputVideoTs, inputVideoStream->time_base);
            const int64_t normalizedVideoUs = normalizeUs(inputVideoUs);

            if (normalizedVideoUs == AV_NOPTS_VALUE) {
                fail("failed to normalize input video timestamp");
                return false;
            }

            /*
             * buffer source 的 time_base 是 inputVideoStream->time_base。
             * 所以送入 filter graph 前，将帧时间戳归一化到 0 起点，
             * 但仍保持在输入视频流 time_base 下。
             */
            decodedFrame->pts = fromUs(normalizedVideoUs, inputVideoStream->time_base);

            if (decodedFrame->pts == AV_NOPTS_VALUE) {
                fail("decoded video frame pts is invalid after normalization");
                return false;
            }

            ret = av_buffersrc_add_frame_flags(
                videoBufferSrcCtx,
                decodedFrame,
                AV_BUFFERSRC_FLAG_KEEP_REF
            );

            if (ret < 0) {
                fail("av_buffersrc_add_frame_flags video failed: " + ffErrorString(ret));
                return false;
            }

            return drainVideoFilterGraph();
            };

        auto drainDecoder = [&]() -> bool {
            while (true) {
                ret = avcodec_receive_frame(decoderCtx, decodedFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    return true;
                }

                if (ret < 0) {
                    fail("avcodec_receive_frame decoder failed: " + ffErrorString(ret));
                    return false;
                }

                const bool ok = processDecodedFrame();

                av_frame_unref(decodedFrame);

                if (!ok) {
                    return false;
                }

                if (m_stopRequested.load()) {
                    return true;
                }
            }
            };

        auto normalizeAudioPacketTimestamp = [&](AVPacket* packet) -> bool {
            if (!packet || !inputAudioStream || !outputAudioStream) {
                return true;
            }

            const AVRational inputTimeBase = inputAudioStream->time_base;
            const AVRational outputTimeBase = outputAudioStream->time_base;

            if (packet->pts != AV_NOPTS_VALUE) {
                const int64_t ptsUs = toUs(packet->pts, inputTimeBase);
                const int64_t normalizedPtsUs = normalizeUs(ptsUs);

                if (normalizedPtsUs == AV_NOPTS_VALUE) {
                    fail("failed to normalize audio packet pts");
                    return false;
                }

                packet->pts = fromUs(normalizedPtsUs, outputTimeBase);
            }

            if (packet->dts != AV_NOPTS_VALUE) {
                const int64_t dtsUs = toUs(packet->dts, inputTimeBase);
                const int64_t normalizedDtsUs = normalizeUs(dtsUs);

                if (normalizedDtsUs == AV_NOPTS_VALUE) {
                    fail("failed to normalize audio packet dts");
                    return false;
                }

                packet->dts = fromUs(normalizedDtsUs, outputTimeBase);
            }

            if (packet->duration > 0) {
                packet->duration = av_rescale_q(
                    packet->duration,
                    inputTimeBase,
                    outputTimeBase
                );
            }

            return true;
            };

        auto updateAudioProgressFromPacket = [&](const AVPacket* packet) {
            if (!packet || !outputAudioStream) {
                return;
            }

            int64_t timestamp = packet->pts;
            if (timestamp == AV_NOPTS_VALUE) {
                timestamp = packet->dts;
            }

            if (timestamp == AV_NOPTS_VALUE) {
                return;
            }

            lastWrittenAudioOutTimeMs =
                std::max<int64_t>(
                    lastWrittenAudioOutTimeMs,
                    toUs(timestamp, outputAudioStream->time_base) / 1000
                );
            };

        auto writeAudioPacketCopy = [&](AVPacket* packet) -> bool {
            if (!outputAudioStream || !inputAudioStream) {
                return true;
            }

            packet->stream_index = outputAudioStream->index;

            if (!normalizeAudioPacketTimestamp(packet)) {
                return false;
            }

            if (packet->dts != AV_NOPTS_VALUE) {
                if (lastWrittenAudioDts != AV_NOPTS_VALUE &&
                    packet->dts <= lastWrittenAudioDts) {
                    std::ostringstream oss;
                    oss << "audio packet dts is not strictly increasing: current="
                        << packet->dts
                        << ", last="
                        << lastWrittenAudioDts;

                    fail(oss.str());
                    return false;
                }

                lastWrittenAudioDts = packet->dts;
            }

            if (packet->pts != AV_NOPTS_VALUE &&
                packet->dts != AV_NOPTS_VALUE &&
                packet->pts < packet->dts) {
                std::ostringstream oss;
                oss << "audio packet pts is smaller than dts: pts="
                    << packet->pts
                    << ", dts="
                    << packet->dts;

                fail(oss.str());
                return false;
            }

            updateAudioProgressFromPacket(packet);

            ret = av_interleaved_write_frame(outputFmtCtx, packet);
            if (ret < 0) {
                fail("av_interleaved_write_frame audio failed: " + ffErrorString(ret));
                return false;
            }

            return true;
            };

        auto getDecodedAudioTimestamp = [&]() -> int64_t {
            if (!decodedAudioFrame) {
                return AV_NOPTS_VALUE;
            }

            if (decodedAudioFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
                return decodedAudioFrame->best_effort_timestamp;
            }

            if (decodedAudioFrame->pts != AV_NOPTS_VALUE) {
                return decodedAudioFrame->pts;
            }

            if (decodedAudioFrame->pkt_dts != AV_NOPTS_VALUE) {
                return decodedAudioFrame->pkt_dts;
            }

            return AV_NOPTS_VALUE;
            };

        auto ensureInitialAudioPts = [&]() -> bool {
            if (!audioEncoderCtx) {
                return true;
            }

            if (nextAudioPts != AV_NOPTS_VALUE) {
                return true;
            }

            const int64_t inputAudioTs = getDecodedAudioTimestamp();
            if (inputAudioTs == AV_NOPTS_VALUE) {
                nextAudioPts = 0;
                return true;
            }

            const int64_t inputAudioUs = toUs(inputAudioTs, inputAudioStream->time_base);
            const int64_t normalizedAudioUs = normalizeUs(inputAudioUs);

            if (normalizedAudioUs == AV_NOPTS_VALUE) {
                fail("failed to normalize input audio timestamp");
                return false;
            }

            nextAudioPts = fromUs(normalizedAudioUs, audioEncoderCtx->time_base);

            if (nextAudioPts == AV_NOPTS_VALUE || nextAudioPts < 0) {
                nextAudioPts = 0;
            }

            return true;
            };

        auto writeEncodedAudioPackets = [&](AVFrame* frame) -> bool {
            if (!audioEncoderCtx || !outputAudioStream) {
                return true;
            }

            int sendRet = avcodec_send_frame(audioEncoderCtx, frame);
            if (sendRet < 0) {
                fail("avcodec_send_frame audio encoder failed: " + ffErrorString(sendRet));
                return false;
            }

            while (true) {
                AVPacket* encodedPacket = av_packet_alloc();
                if (!encodedPacket) {
                    fail("av_packet_alloc audio encodedPacket failed");
                    return false;
                }

                int receiveRet = avcodec_receive_packet(audioEncoderCtx, encodedPacket);

                if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
                    av_packet_free(&encodedPacket);
                    break;
                }

                if (receiveRet < 0) {
                    const std::string error = "avcodec_receive_packet audio encoder failed: " +
                        ffErrorString(receiveRet);
                    av_packet_free(&encodedPacket);
                    fail(error);
                    return false;
                }

                encodedPacket->stream_index = outputAudioStream->index;

                av_packet_rescale_ts(
                    encodedPacket,
                    audioEncoderCtx->time_base,
                    outputAudioStream->time_base
                );

                if (encodedPacket->dts != AV_NOPTS_VALUE) {
                    if (lastWrittenAudioDts != AV_NOPTS_VALUE &&
                        encodedPacket->dts <= lastWrittenAudioDts) {
                        std::ostringstream oss;
                        oss << "encoded audio packet dts is not strictly increasing: current="
                            << encodedPacket->dts
                            << ", last="
                            << lastWrittenAudioDts;

                        av_packet_free(&encodedPacket);
                        fail(oss.str());
                        return false;
                    }

                    lastWrittenAudioDts = encodedPacket->dts;
                }

                if (encodedPacket->pts != AV_NOPTS_VALUE &&
                    encodedPacket->dts != AV_NOPTS_VALUE &&
                    encodedPacket->pts < encodedPacket->dts) {
                    std::ostringstream oss;
                    oss << "encoded audio packet pts is smaller than dts: pts="
                        << encodedPacket->pts
                        << ", dts="
                        << encodedPacket->dts;

                    av_packet_free(&encodedPacket);
                    fail(oss.str());
                    return false;
                }

                updateAudioProgressFromPacket(encodedPacket);

                ret = av_interleaved_write_frame(outputFmtCtx, encodedPacket);
                av_packet_free(&encodedPacket);

                if (ret < 0) {
                    fail("av_interleaved_write_frame encoded audio failed: " + ffErrorString(ret));
                    return false;
                }

                ++encodedAudioPacketCount;
            }

            return true;
            };

        auto encodeAudioFifo = [&](bool flushAll) -> bool {
            if (!audioFifo || !audioEncoderCtx) {
                return true;
            }

            const int frameSize = audioEncoderCtx->frame_size > 0
                ? audioEncoderCtx->frame_size
                : 1024;

            while (av_audio_fifo_size(audioFifo) >= frameSize ||
                (flushAll && av_audio_fifo_size(audioFifo) > 0)) {
                const int availableSamples = av_audio_fifo_size(audioFifo);
                const int samplesToRead = flushAll
                    ? std::min(frameSize, availableSamples)
                    : frameSize;

                AVFrame* audioFrame = av_frame_alloc();
                if (!audioFrame) {
                    fail("av_frame_alloc encoded audio frame failed");
                    return false;
                }

                audioFrame->nb_samples = samplesToRead;
                audioFrame->format = audioEncoderCtx->sample_fmt;
                audioFrame->sample_rate = audioEncoderCtx->sample_rate;
                audioFrame->pts = nextAudioPts == AV_NOPTS_VALUE ? 0 : nextAudioPts;

                if (!setFrameAudioLayoutFromCodecContext(audioFrame, audioEncoderCtx)) {
                    av_frame_free(&audioFrame);
                    fail("set encoded audio frame channel layout failed");
                    return false;
                }

                ret = av_frame_get_buffer(audioFrame, 0);
                if (ret < 0) {
                    av_frame_free(&audioFrame);
                    fail("av_frame_get_buffer encoded audio frame failed: " + ffErrorString(ret));
                    return false;
                }

                const int readSamples = av_audio_fifo_read(
                    audioFifo,
                    reinterpret_cast<void**>(audioFrame->extended_data),
                    samplesToRead
                );

                if (readSamples < samplesToRead) {
                    av_frame_free(&audioFrame);
                    fail("av_audio_fifo_read failed");
                    return false;
                }

                const bool ok = writeEncodedAudioPackets(audioFrame);

                nextAudioPts = audioFrame->pts + audioFrame->nb_samples;

                av_frame_free(&audioFrame);

                if (!ok) {
                    return false;
                }
            }

            return true;
            };

        auto pushDecodedAudioFrameToFifo = [&]() -> bool {
            if (!decodedAudioFrame || !swrCtx || !audioFifo || !audioEncoderCtx || !audioDecoderCtx) {
                return true;
            }

            if (!ensureInitialAudioPts()) {
                return false;
            }

            const int64_t delay = swr_get_delay(swrCtx, audioDecoderCtx->sample_rate);
            const int dstNbSamples = static_cast<int>(av_rescale_rnd(
                delay + decodedAudioFrame->nb_samples,
                audioEncoderCtx->sample_rate,
                audioDecoderCtx->sample_rate,
                AV_ROUND_UP
            ));

            if (dstNbSamples <= 0) {
                return true;
            }

            AVFrame* convertedFrame = av_frame_alloc();
            if (!convertedFrame) {
                fail("av_frame_alloc converted audio frame failed");
                return false;
            }

            convertedFrame->nb_samples = dstNbSamples;
            convertedFrame->format = audioEncoderCtx->sample_fmt;
            convertedFrame->sample_rate = audioEncoderCtx->sample_rate;

            if (!setFrameAudioLayoutFromCodecContext(convertedFrame, audioEncoderCtx)) {
                av_frame_free(&convertedFrame);
                fail("set converted audio frame channel layout failed");
                return false;
            }

            ret = av_frame_get_buffer(convertedFrame, 0);
            if (ret < 0) {
                av_frame_free(&convertedFrame);
                fail("av_frame_get_buffer converted audio frame failed: " + ffErrorString(ret));
                return false;
            }

            const auto srcData =
                const_cast<const uint8_t**>(decodedAudioFrame->extended_data);

            const int convertedSamples = swr_convert(
                swrCtx,
                convertedFrame->extended_data,
                dstNbSamples,
                srcData,
                decodedAudioFrame->nb_samples
            );

            if (convertedSamples < 0) {
                av_frame_free(&convertedFrame);
                fail("swr_convert failed: " + ffErrorString(convertedSamples));
                return false;
            }

            convertedFrame->nb_samples = convertedSamples;

            if (convertedSamples > 0) {
                ret = av_audio_fifo_realloc(
                    audioFifo,
                    av_audio_fifo_size(audioFifo) + convertedSamples
                );

                if (ret < 0) {
                    av_frame_free(&convertedFrame);
                    fail("av_audio_fifo_realloc failed: " + ffErrorString(ret));
                    return false;
                }

                const int writtenSamples = av_audio_fifo_write(
                    audioFifo,
                    reinterpret_cast<void**>(convertedFrame->extended_data),
                    convertedSamples
                );

                if (writtenSamples < convertedSamples) {
                    av_frame_free(&convertedFrame);
                    fail("av_audio_fifo_write failed");
                    return false;
                }
            }

            av_frame_free(&convertedFrame);

            return encodeAudioFifo(false);
            };

        auto drainAudioDecoder = [&]() -> bool {
            if (!audioDecoderCtx || !decodedAudioFrame) {
                return true;
            }

            while (true) {
                ret = avcodec_receive_frame(audioDecoderCtx, decodedAudioFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    return true;
                }

                if (ret < 0) {
                    fail("avcodec_receive_frame audio decoder failed: " + ffErrorString(ret));
                    return false;
                }

                const bool ok = pushDecodedAudioFrameToFifo();

                av_frame_unref(decodedAudioFrame);

                if (!ok) {
                    return false;
                }

                if (m_stopRequested.load()) {
                    return true;
                }
            }
            };

        auto sendAudioPacketToDecoder = [&](AVPacket* packet) -> bool {
            if (!audioDecoderCtx) {
                return true;
            }

            ret = avcodec_send_packet(audioDecoderCtx, packet);
            if (ret < 0) {
                fail("avcodec_send_packet audio decoder failed: " + ffErrorString(ret));
                return false;
            }

            return drainAudioDecoder();
            };

        /*
         * 9. 主转码循环
         */
        while (!m_stopRequested.load()) {
            ret = av_read_frame(inputFmtCtx, inputPacket);

            if (ret == AVERROR_EOF) {
                break;
            }

            if (ret < 0) {
                fail("av_read_frame failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            if (inputPacket->stream_index == videoStreamIndex) {
                ret = avcodec_send_packet(decoderCtx, inputPacket);
                av_packet_unref(inputPacket);

                if (ret < 0) {
                    fail("avcodec_send_packet decoder failed: " + ffErrorString(ret));
                    cleanup();
                    return;
                }

                if (!drainDecoder()) {
                    cleanup();
                    return;
                }
            }
            else if (inputPacket->stream_index == audioStreamIndex &&
                outputAudioStream &&
                m_config.audioMode == AudioMode::CopySelected) {
                if (!writeAudioPacketCopy(inputPacket)) {
                    av_packet_unref(inputPacket);
                    cleanup();
                    return;
                }

                /*
                 * av_interleaved_write_frame 成功后 packet 内容已经被 muxer 接管或消耗。
                 * 这里仍然 unref 是安全的。
                 */
                av_packet_unref(inputPacket);
            }
            else if (inputPacket->stream_index == audioStreamIndex &&
                audioDecoderCtx &&
                m_config.audioMode == AudioMode::EncodeSelected) {
                const bool ok = sendAudioPacketToDecoder(inputPacket);
                av_packet_unref(inputPacket);

                if (!ok) {
                    cleanup();
                    return;
                }
            }
            else {
                av_packet_unref(inputPacket);
            }
        }

        /*
         * 10. flush decoder
         */
        if (!m_stopRequested.load()) {
            ret = avcodec_send_packet(decoderCtx, nullptr);
            if (ret < 0) {
                fail("avcodec_send_packet decoder flush failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            if (!drainDecoder()) {
                cleanup();
                return;
            }
        }

        /*
         * 10.1 flush audio decoder / resampler / encoder
         */
        if (!m_stopRequested.load() && audioDecoderCtx && audioEncoderCtx) {
            ret = avcodec_send_packet(audioDecoderCtx, nullptr);
            if (ret < 0) {
                fail("avcodec_send_packet audio decoder flush failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            if (!drainAudioDecoder()) {
                cleanup();
                return;
            }

            /*
             * 让 swr 输出内部延迟样本。
             */
            while (true) {
                const int64_t delay = swr_get_delay(swrCtx, audioDecoderCtx->sample_rate);
                if (delay <= 0) {
                    break;
                }

                const int dstNbSamples = static_cast<int>(av_rescale_rnd(
                    delay,
                    audioEncoderCtx->sample_rate,
                    audioDecoderCtx->sample_rate,
                    AV_ROUND_UP
                ));

                if (dstNbSamples <= 0) {
                    break;
                }

                AVFrame* convertedFrame = av_frame_alloc();
                if (!convertedFrame) {
                    fail("av_frame_alloc swr flush audio frame failed");
                    cleanup();
                    return;
                }

                convertedFrame->nb_samples = dstNbSamples;
                convertedFrame->format = audioEncoderCtx->sample_fmt;
                convertedFrame->sample_rate = audioEncoderCtx->sample_rate;

                if (!setFrameAudioLayoutFromCodecContext(convertedFrame, audioEncoderCtx)) {
                    av_frame_free(&convertedFrame);
                    fail("set swr flush audio frame channel layout failed");
                    cleanup();
                    return;
                }

                ret = av_frame_get_buffer(convertedFrame, 0);
                if (ret < 0) {
                    av_frame_free(&convertedFrame);
                    fail("av_frame_get_buffer swr flush audio frame failed: " + ffErrorString(ret));
                    cleanup();
                    return;
                }

                const int convertedSamples = swr_convert(
                    swrCtx,
                    convertedFrame->extended_data,
                    dstNbSamples,
                    nullptr,
                    0
                );

                if (convertedSamples < 0) {
                    av_frame_free(&convertedFrame);
                    fail("swr_convert flush failed: " + ffErrorString(convertedSamples));
                    cleanup();
                    return;
                }

                convertedFrame->nb_samples = convertedSamples;

                if (convertedSamples <= 0) {
                    av_frame_free(&convertedFrame);
                    break;
                }

                ret = av_audio_fifo_realloc(
                    audioFifo,
                    av_audio_fifo_size(audioFifo) + convertedSamples
                );

                if (ret < 0) {
                    av_frame_free(&convertedFrame);
                    fail("av_audio_fifo_realloc swr flush failed: " + ffErrorString(ret));
                    cleanup();
                    return;
                }

                const int writtenSamples = av_audio_fifo_write(
                    audioFifo,
                    reinterpret_cast<void**>(convertedFrame->extended_data),
                    convertedSamples
                );

                av_frame_free(&convertedFrame);

                if (writtenSamples < convertedSamples) {
                    fail("av_audio_fifo_write swr flush failed");
                    cleanup();
                    return;
                }
            }

            if (!encodeAudioFifo(true)) {
                cleanup();
                return;
            }

            if (!writeEncodedAudioPackets(nullptr)) {
                cleanup();
                return;
            }
        }

        /*
         * 10.5 flush video filter graph
         */
        if (!m_stopRequested.load()) {
            ret = av_buffersrc_add_frame_flags(videoBufferSrcCtx, nullptr, 0);
            if (ret < 0) {
                fail("av_buffersrc_add_frame_flags video EOF failed: " + ffErrorString(ret));
                cleanup();
                return;
            }

            if (!drainVideoFilterGraph()) {
                cleanup();
                return;
            }
        }

        /*
         * 11. flush video encoder
         */
        if (!m_stopRequested.load()) {
            if (!writeEncodedVideoPackets(nullptr)) {
                cleanup();
                return;
            }
        }

        /*
         * 12. 写 trailer
         */
        ret = av_write_trailer(outputFmtCtx);
        if (ret < 0) {
            fail("av_write_trailer failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        emitProgress(m_stopRequested.load() ? "stopped" : "finished");

        cleanup();
    }

    void FFmpegTranscoder::setLastError(const std::string& error)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = error;
    }

    void FFmpegTranscoder::clearLastError()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError.clear();
    }

} // namespace media
