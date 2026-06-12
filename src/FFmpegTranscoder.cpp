#include "media_transcode/FFmpegTranscoder.h"
#include "internal/FFmpegUtils.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegVideoFilterGraph.h"
#include "internal/FFmpegVideoPipeline.h"
#include "internal/FFmpegAudioPipeline.h"

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
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/version.h>
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

        ffmpeg::VideoFilterGraph videoFilterGraph;
        ffmpeg::FFmpegVideoPipeline videoPipeline;
        ffmpeg::FFmpegAudioPipeline audioPipeline;

        AVPacket* inputPacket = nullptr;
        AVFrame* decodedFrame = nullptr;
        AVFrame* filteredFrame = nullptr;

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

        AVStream* inputVideoStream = nullptr;
        AVStream* inputAudioStream = nullptr;
        AVStream* outputVideoStream = nullptr;

        int64_t encodedVideoPacketCount = 0;
        int64_t encodedAudioPacketCount = 0;

        ffmpeg::TimelineNormalizer timeline;

        int64_t lastSubmittedVideoPts = AV_NOPTS_VALUE;
        int64_t lastWrittenVideoOutTimeMs = 0;
        int64_t lastWrittenAudioOutTimeMs = 0;

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
            videoFilterGraph.reset();
            videoPipeline.reset();
            audioPipeline.reset();

            if (filteredFrame) {
                av_frame_free(&filteredFrame);
            }

            if (decodedFrame) {
                av_frame_free(&decodedFrame);
            }

            if (inputPacket) {
                av_packet_free(&inputPacket);
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
                fail("invalid output video size");
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
         * 6. 音频处理交给 FFmpegAudioPipeline：
         *
         * - None：不输出音频。
         * - CopySelected：Pipeline 创建输出音频流并复制 packet。
         * - EncodeSelected：Pipeline 内部完成解码、重采样、FIFO、AAC 编码和写包。
         */
        if (inputAudioStream && m_config.audioMode != AudioMode::None) {
            ffmpeg::FFmpegAudioPipeline::Config audioPipelineConfig;
            audioPipelineConfig.mode = m_config.audioMode;
            audioPipelineConfig.inputAudioStream = inputAudioStream;
            audioPipelineConfig.outputFmtCtx = outputFmtCtx;
            audioPipelineConfig.timeline = &timeline;
            audioPipelineConfig.audioBitrateKbps = m_config.audioBitrateKbps;

            std::string audioPipelineError;
            if (!audioPipeline.initialize(audioPipelineConfig, &audioPipelineError)) {
                fail(audioPipelineError);
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

        if (!inputPacket || !decodedFrame || !filteredFrame) {
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

        auto onAudioPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
            encodedAudioPacketCount = packetCount;
            lastWrittenAudioOutTimeMs = outTimeMs;

            if (encodedAudioPacketCount == 1 ||
                encodedAudioPacketCount % 25 == 0) {
                emitProgress("transcoding");
            }
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
                audioPipeline.outputStream()) {
                std::string audioPipelineError;
                const bool ok = audioPipeline.processPacket(
                    inputPacket,
                    &audioPipelineError,
                    onAudioPacketWritten
                );

                av_packet_unref(inputPacket);

                if (!ok) {
                    fail(audioPipelineError);
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
         * 10.1 flush audio pipeline
         */
        if (!m_stopRequested.load()) {
            std::string audioPipelineError;
            if (!audioPipeline.flush(&audioPipelineError, onAudioPacketWritten)) {
                fail(audioPipelineError);
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
