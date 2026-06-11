#include "media_transcode/FFmpegTranscoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/version_major.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
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

        SwsContext* swsCtx = nullptr;

        AVPacket* inputPacket = nullptr;
        AVFrame* decodedFrame = nullptr;
        AVFrame* convertedFrame = nullptr;

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

        AVStream* inputVideoStream = nullptr;
        AVStream* inputAudioStream = nullptr;

        AVStream* outputVideoStream = nullptr;
        AVStream* outputAudioStream = nullptr;

        int64_t encodedVideoPacketCount = 0;

        int64_t timelineStartUs = AV_NOPTS_VALUE;
        int64_t lastSubmittedVideoPts = AV_NOPTS_VALUE;
        int64_t lastWrittenVideoDts = AV_NOPTS_VALUE;
        int64_t lastWrittenAudioDts = AV_NOPTS_VALUE;

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
            info.outTimeMs = encoderCtx && encoderCtx->framerate.num > 0
                ? static_cast<int64_t>(encodedVideoPacketCount * 1000.0 *
                    encoderCtx->framerate.den /
                    encoderCtx->framerate.num)
                : 0;

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
            if (convertedFrame) {
                av_frame_free(&convertedFrame);
            }

            if (decodedFrame) {
                av_frame_free(&decodedFrame);
            }

            if (inputPacket) {
                av_packet_free(&inputPacket);
            }

            if (swsCtx) {
                sws_freeContext(swsCtx);
                swsCtx = nullptr;
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

            const int outputFps = chooseOutputFps(m_config, inputVideoStream);

            int outputWidth = m_config.width > 0 ? m_config.width : decoderCtx->width;
            int outputHeight = m_config.height > 0 ? m_config.height : decoderCtx->height;

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
             * V2 时间戳策略：
             *
             * 第一版使用 1/fps + frameIndex++，这是人为生成时间轴。
             * 第二版改为优先沿用输入视频流 time_base。
             *
             * 这样 decodedFrame 的真实时间戳可以直接 rescale 到编码器时间轴，
             * 不会因为输出 fps 四舍五入导致视频时长变化。
             */
            AVRational encoderTimeBase = inputVideoStream->time_base;

            if (encoderTimeBase.num <= 0 || encoderTimeBase.den <= 0) {
                encoderTimeBase = AVRational{ 1, outputFps };
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

            /*
             * 一些硬编码器对 time_base / framerate 比较敏感。
             * 这里统一用 1/fps，后面 frame->pts 递增。
             */
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
         * 6. 音频第一版采用 copy。
         *    注意：如果输入音频编码不被输出容器支持，写 header 或写 packet 可能失败。
         *    下一版可以加 AAC 重编码。
         */
        if (inputAudioStream && m_config.audioMode != AudioMode::None) {
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
        convertedFrame = av_frame_alloc();

        if (!inputPacket || !decodedFrame || !convertedFrame) {
            fail("av_packet_alloc / av_frame_alloc failed");
            cleanup();
            return;
        }

        convertedFrame->format = encoderCtx->pix_fmt;
        convertedFrame->width = encoderCtx->width;
        convertedFrame->height = encoderCtx->height;

        ret = av_frame_get_buffer(convertedFrame, 32);
        if (ret < 0) {
            fail("av_frame_get_buffer convertedFrame failed: " + ffErrorString(ret));
            cleanup();
            return;
        }

        swsCtx = sws_getContext(
            decoderCtx->width,
            decoderCtx->height,
            decoderCtx->pix_fmt,
            encoderCtx->width,
            encoderCtx->height,
            encoderCtx->pix_fmt,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );

        if (!swsCtx) {
            fail("sws_getContext failed");
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
            ret = av_frame_make_writable(convertedFrame);
            if (ret < 0) {
                fail("av_frame_make_writable failed: " + ffErrorString(ret));
                return false;
            }

            sws_scale(
                swsCtx,
                decodedFrame->data,
                decodedFrame->linesize,
                0,
                decoderCtx->height,
                convertedFrame->data,
                convertedFrame->linesize
            );

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

            convertedFrame->pts = fromUs(normalizedVideoUs, encoderCtx->time_base);

            /*
             * 第二版原则：
             * 输入时间戳规范时，输出自然单调；
             * 输入时间戳不规范时，直接报错，不再通过 frameIndex++ 或 delta 修正伪同步。
             */
            if (convertedFrame->pts == AV_NOPTS_VALUE) {
                fail("converted video frame pts is invalid after rescale");
                return false;
            }

            if (lastSubmittedVideoPts != AV_NOPTS_VALUE &&
                convertedFrame->pts <= lastSubmittedVideoPts) {
                std::ostringstream oss;
                oss << "input video timestamp is not strictly increasing after normalization: current="
                    << convertedFrame->pts
                    << ", last="
                    << lastSubmittedVideoPts;

                fail(oss.str());
                return false;
            }

            lastSubmittedVideoPts = convertedFrame->pts;

            return writeEncodedVideoPackets(convertedFrame);
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

            ret = av_interleaved_write_frame(outputFmtCtx, packet);
            if (ret < 0) {
                fail("av_interleaved_write_frame audio failed: " + ffErrorString(ret));
                return false;
            }

            return true;
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
                m_config.audioMode != AudioMode::None) {
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
         * 11. flush encoder
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