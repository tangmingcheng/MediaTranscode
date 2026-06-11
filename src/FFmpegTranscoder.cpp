#include "media_transcode/FFmpegTranscoder.h"
#include "internal/FFmpegUtils.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/version.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace media {
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
            fail("avformat_open_input failed: " + ffmpeg::errorString(ret));
            cleanup();
            return;
        }

        ret = avformat_find_stream_info(inputFmtCtx, nullptr);
        if (ret < 0) {
            fail("avformat_find_stream_info failed: " + ffmpeg::errorString(ret));
            cleanup();
            return;
        }

        /*
         * 2. 查找视频流 / 音频流
         */
        ret = av_find_best_stream(inputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (ret < 0) {
            fail("av_find_best_stream video failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_parameters_to_context decoder failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }

            ret = avcodec_open2(decoderCtx, decoder, nullptr);
            if (ret < 0) {
                fail("avcodec_open2 decoder failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }
        }

        /*
         * 4. 创建输出上下文
         */
        ret = avformat_alloc_output_context2(&outputFmtCtx, nullptr, nullptr, m_config.outputUrl.c_str());
        if (ret < 0 || !outputFmtCtx) {
            fail("avformat_alloc_output_context2 failed: " + ffmpeg::errorString(ret));
            cleanup();
            return;
        }

        /*
         * 5. 创建并打开视频编码器
         */
        {
            const char* encoderName = ffmpeg::preferredVideoEncoderName(m_config.videoCodec);
            const AVCodec* encoder = nullptr;

            if (encoderName) {
                encoder = avcodec_find_encoder_by_name(encoderName);
            }

            if (!encoder) {
                const AVCodecID codecId = ffmpeg::fallbackVideoCodecId(m_config.videoCodec);
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

            outputFps = ffmpeg::chooseOutputFps(m_config, inputVideoStream);
            enableConstantFps = m_config.fps > 0;

            outputWidth = m_config.width > 0 ? m_config.width : decoderCtx->width;
            outputHeight = m_config.height > 0 ? m_config.height : decoderCtx->height;

            outputWidth = ffmpeg::normalizeEvenSize(outputWidth);
            outputHeight = ffmpeg::normalizeEvenSize(outputHeight);

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
            encoderCtx->pix_fmt = ffmpeg::chooseVideoEncoderPixelFormat(encoder);

            encoderCtx->bit_rate = static_cast<int64_t>(std::max(1, m_config.videoBitrateKbps)) * 1000;
            encoderCtx->gop_size = std::max(10, outputFps * 2);
            encoderCtx->max_b_frames = 0;

            if (outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            ffmpeg::setVideoEncoderOptions(encoderCtx, encoder);

            ret = avcodec_open2(encoderCtx, encoder, nullptr);
            if (ret < 0) {
                fail(std::string("avcodec_open2 encoder failed [") +
                    (encoder->name ? encoder->name : "unknown") + "]: " +
                    ffmpeg::errorString(ret));
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
                fail("avcodec_parameters_from_context video failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_parameters_copy audio failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_parameters_to_context audio decoder failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }

            audioDecoderCtx->pkt_timebase = inputAudioStream->time_base;

            ret = avcodec_open2(audioDecoderCtx, audioDecoder, nullptr);
            if (ret < 0) {
                fail("avcodec_open2 audio decoder failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }

            if (!ffmpeg::ensureAudioDecoderChannelLayout(audioDecoderCtx)) {
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

            if (!ffmpeg::copyAudioChannelLayoutToEncoder(audioEncoderCtx, audioDecoderCtx)) {
                fail("copy audio channel layout to encoder failed");
                cleanup();
                return;
            }

            audioEncoderCtx->sample_rate =
                ffmpeg::chooseAudioSampleRate(audioEncoder, audioDecoderCtx->sample_rate);
            audioEncoderCtx->sample_fmt = ffmpeg::chooseAudioSampleFormat(audioEncoder);
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
                    ffmpeg::errorString(ret));
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
                fail("avcodec_parameters_from_context audio failed: " + ffmpeg::errorString(ret));
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
                fail("swr_alloc_set_opts2 failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }
#else
            swrCtx = swr_alloc_set_opts(
                nullptr,
                ffmpeg::oldAudioChannelLayout(audioEncoderCtx),
                audioEncoderCtx->sample_fmt,
                audioEncoderCtx->sample_rate,
                ffmpeg::oldAudioChannelLayout(audioDecoderCtx),
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
                fail("swr_init failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }

            const int outputChannels = ffmpeg::audioChannelCount(audioEncoderCtx);
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
                fail("avio_open output failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }
        }

        ret = avformat_write_header(outputFmtCtx, nullptr);
        if (ret < 0) {
            fail("avformat_write_header failed: " + ffmpeg::errorString(ret));
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
                fail("avfilter_graph_create_filter buffer failed: " + ffmpeg::errorString(ret));
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
                fail("avfilter_graph_create_filter buffersink failed: " + ffmpeg::errorString(ret));
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
                fail("avfilter_graph_parse_ptr failed [" + filterDesc + "]: " + ffmpeg::errorString(ret));
                return false;
            }

            ret = avfilter_graph_config(videoFilterGraph, nullptr);
            if (ret < 0) {
                fail("avfilter_graph_config failed [" + filterDesc + "]: " + ffmpeg::errorString(ret));
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
                fail("avcodec_send_frame encoder failed: " + ffmpeg::errorString(sendRet));
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
                        ffmpeg::errorString(receiveRet);
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
                    fail("av_interleaved_write_frame video failed: " + ffmpeg::errorString(ret));
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
                    fail("av_buffersink_get_frame failed: " + ffmpeg::errorString(ret));
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

                ...