#include "media_transcode/FFmpegTranscoder.h"
#include "internal/FFmpegUtils.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegVideoFilterGraph.h"
#include "internal/FFmpegVideoPipeline.h"
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

        ffmpeg::VideoFilterGraph videoFilterGraph;
        ffmpeg::FFmpegVideoPipeline videoPipeline;

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

        ffmpeg::TimelineNormalizer timeline;

        int64_t lastSubmittedVideoPts = AV_NOPTS_VALUE;
        int64_t lastWrittenAudioDts = AV_NOPTS_VALUE;
        int64_t lastWrittenVideoOutTimeMs = 0;
        int64_t lastWrittenAudioOutTimeMs = 0;

        int64_t nextAudioPts = AV_NOPTS_VALUE;

        int outputFps = 0;
        int outputWidth = 0;
        int outputHeight = 0;
        bool enableConstantFps = false;

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

            videoFilterGraph.reset();
            videoPipeline.reset();

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

        timeline.initStartFromFormat(inputFmtCtx, inputVideoStream, inputAudioStream);

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

        {
            ffmpeg::VideoFilterGraph::Config videoFilterConfig;
            videoFilterConfig.decoderCtx = decoderCtx;
            videoFilterConfig.encoderCtx = encoderCtx;
            videoFilterConfig.inputStream = inputVideoStream;
            videoFilterConfig.outputFps = outputFps;
            videoFilterConfig.enableConstantFps = enableConstantFps;

            std::string videoFilterError;
            if (!videoFilterGraph.initialize(videoFilterConfig, &videoFilterError)) {
                fail(videoFilterError);
                cleanup();
                return;
            }
        }

        {
            ffmpeg::FFmpegVideoPipeline::Config videoPipelineConfig;
            videoPipelineConfig.encoderCtx = encoderCtx;
            videoPipelineConfig.outputFmtCtx = outputFmtCtx;
            videoPipelineConfig.outputVideoStream = outputVideoStream;

            std::string videoPipelineError;
            if (!videoPipeline.initialize(videoPipelineConfig, &videoPipelineError)) {
                fail(videoPipelineError);
                cleanup();
                return;
            }
        }

        auto writeEncodedVideoPackets = [&](AVFrame* frame) -> bool {
            std::string videoPipelineError;

            if (!videoPipeline.sendFrame(frame, &videoPipelineError)) {
                fail(videoPipelineError);
                return false;
            }

            const int writtenPackets = videoPipeline.receiveAndWritePackets(
                &videoPipelineError,
                [&](int64_t packetCount, int64_t outTimeMs) {
                    encodedVideoPacketCount = packetCount;
                    lastWrittenVideoOutTimeMs = outTimeMs;

                    if (encodedVideoPacketCount == 1 ||
                        encodedVideoPacketCount % 25 == 0) {
                        emitProgress("transcoding");
                    }
                }
            );

            if (writtenPackets < 0) {
                fail(videoPipelineError);
                return false;
            }

            return true;
        };

        auto drainVideoFilterGraph = [&]() -> bool {
            while (true) {
                std::string videoFilterError;
                const int receiveRet = videoFilterGraph.receiveFrame(filteredFrame, &videoFilterError);

                if (receiveRet == 0) {
                    return true;
                }

                if (receiveRet < 0) {
                    fail(videoFilterError);
                    return false;
                }

                const AVRational filterTimeBase = videoFilterGraph.sinkTimeBase();

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

            const int64_t inputVideoUs = ffmpeg::TimelineNormalizer::toUs(inputVideoTs, inputVideoStream->time_base);
            const int64_t normalizedVideoUs = timeline.normalizeUs(inputVideoUs);

            if (normalizedVideoUs == AV_NOPTS_VALUE) {
                fail("failed to normalize input video timestamp");
                return false;
            }

            /*
             * buffer source 的 time_base 是 inputVideoStream->time_base。
             * 所以送入 filter graph 前，将帧时间戳归一化到 0 起点，
             * 但仍保持在输入视频流 time_base 下。
             */
            decodedFrame->pts = ffmpeg::TimelineNormalizer::fromUs(normalizedVideoUs, inputVideoStream->time_base);

            if (decodedFrame->pts == AV_NOPTS_VALUE) {
                fail("decoded video frame pts is invalid after normalization");
                return false;
            }

            {
                std::string videoFilterError;
                if (!videoFilterGraph.sendFrame(decodedFrame, &videoFilterError)) {
                    fail(videoFilterError);
                    return false;
                }
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
                    fail("avcodec_receive_frame decoder failed: " + ffmpeg::errorString(ret));
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
                const int64_t ptsUs = ffmpeg::TimelineNormalizer::toUs(packet->pts, inputTimeBase);
                const int64_t normalizedPtsUs = timeline.normalizeUs(ptsUs);

                if (normalizedPtsUs == AV_NOPTS_VALUE) {
                    fail("failed to normalize audio packet pts");
                    return false;
                }

                packet->pts = ffmpeg::TimelineNormalizer::fromUs(normalizedPtsUs, outputTimeBase);
            }

            if (packet->dts != AV_NOPTS_VALUE) {
                const int64_t dtsUs = ffmpeg::TimelineNormalizer::toUs(packet->dts, inputTimeBase);
                const int64_t normalizedDtsUs = timeline.normalizeUs(dtsUs);

                if (normalizedDtsUs == AV_NOPTS_VALUE) {
                    fail("failed to normalize audio packet dts");
                    return false;
                }

                packet->dts = ffmpeg::TimelineNormalizer::fromUs(normalizedDtsUs, outputTimeBase);
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
                    ffmpeg::TimelineNormalizer::toUs(timestamp, outputAudioStream->time_base) / 1000
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
                fail("av_interleaved_write_frame audio failed: " + ffmpeg::errorString(ret));
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

            const int64_t inputAudioUs = ffmpeg::TimelineNormalizer::toUs(inputAudioTs, inputAudioStream->time_base);
            const int64_t normalizedAudioUs = timeline.normalizeUs(inputAudioUs);

            if (normalizedAudioUs == AV_NOPTS_VALUE) {
                fail("failed to normalize input audio timestamp");
                return false;
            }

            nextAudioPts = ffmpeg::TimelineNormalizer::fromUs(normalizedAudioUs, audioEncoderCtx->time_base);

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
                fail("avcodec_send_frame audio encoder failed: " + ffmpeg::errorString(sendRet));
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
                        ffmpeg::errorString(receiveRet);
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
                    fail("av_interleaved_write_frame encoded audio failed: " + ffmpeg::errorString(ret));
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

                if (!ffmpeg::setFrameAudioLayoutFromCodecContext(audioFrame, audioEncoderCtx)) {
                    av_frame_free(&audioFrame);
                    fail("set encoded audio frame channel layout failed");
                    return false;
                }

                ret = av_frame_get_buffer(audioFrame, 0);
                if (ret < 0) {
                    av_frame_free(&audioFrame);
                    fail("av_frame_get_buffer encoded audio frame failed: " + ffmpeg::errorString(ret));
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

            if (!ffmpeg::setFrameAudioLayoutFromCodecContext(convertedFrame, audioEncoderCtx)) {
                av_frame_free(&convertedFrame);
                fail("set converted audio frame channel layout failed");
                return false;
            }

            ret = av_frame_get_buffer(convertedFrame, 0);
            if (ret < 0) {
                av_frame_free(&convertedFrame);
                fail("av_frame_get_buffer converted audio frame failed: " + ffmpeg::errorString(ret));
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
                fail("swr_convert failed: " + ffmpeg::errorString(convertedSamples));
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
                    fail("av_audio_fifo_realloc failed: " + ffmpeg::errorString(ret));
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
                    fail("avcodec_receive_frame audio decoder failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_send_packet audio decoder failed: " + ffmpeg::errorString(ret));
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
                fail("av_read_frame failed: " + ffmpeg::errorString(ret));
                cleanup();
                return;
            }

            if (inputPacket->stream_index == videoStreamIndex) {
                ret = avcodec_send_packet(decoderCtx, inputPacket);
                av_packet_unref(inputPacket);

                if (ret < 0) {
                    fail("avcodec_send_packet decoder failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_send_packet decoder flush failed: " + ffmpeg::errorString(ret));
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
                fail("avcodec_send_packet audio decoder flush failed: " + ffmpeg::errorString(ret));
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

                if (!ffmpeg::setFrameAudioLayoutFromCodecContext(convertedFrame, audioEncoderCtx)) {
                    av_frame_free(&convertedFrame);
                    fail("set swr flush audio frame channel layout failed");
                    cleanup();
                    return;
                }

                ret = av_frame_get_buffer(convertedFrame, 0);
                if (ret < 0) {
                    av_frame_free(&convertedFrame);
                    fail("av_frame_get_buffer swr flush audio frame failed: " + ffmpeg::errorString(ret));
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
                    fail("swr_convert flush failed: " + ffmpeg::errorString(convertedSamples));
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
                    fail("av_audio_fifo_realloc swr flush failed: " + ffmpeg::errorString(ret));
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
            {
                std::string videoFilterError;
                if (!videoFilterGraph.flush(&videoFilterError)) {
                    fail(videoFilterError);
                    cleanup();
                    return;
                }
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
            fail("av_write_trailer failed: " + ffmpeg::errorString(ret));
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
