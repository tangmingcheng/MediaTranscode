#include "media_transcode/FFmpegTranscoder.h"

#include "internal/FFmpegAudioPipeline.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"
#include "internal/FFmpegVideoTranscodePipeline.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
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
        AVPacket* inputPacket = nullptr;

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

        AVStream* inputVideoStream = nullptr;
        AVStream* inputAudioStream = nullptr;

        ffmpeg::TimelineNormalizer timeline;
        ffmpeg::FFmpegVideoTranscodePipeline videoPipeline;
        ffmpeg::FFmpegAudioPipeline audioPipeline;

        int64_t encodedVideoPacketCount = 0;
        int64_t encodedAudioPacketCount = 0;
        int64_t lastWrittenVideoOutTimeMs = 0;
        int64_t lastWrittenAudioOutTimeMs = 0;

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

            if (info.outTimeMs <= 0) {
                info.outTimeMs = videoPipeline.estimatedOutTimeMs();
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
            audioPipeline.reset();
            videoPipeline.reset();

            if (inputPacket) {
                av_packet_free(&inputPacket);
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

        int ret = avformat_open_input(&inputFmtCtx, m_config.inputUrl.c_str(), nullptr, nullptr);
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

        ret = avformat_alloc_output_context2(&outputFmtCtx, nullptr, nullptr, m_config.outputUrl.c_str());
        if (ret < 0 || !outputFmtCtx) {
            fail("avformat_alloc_output_context2 failed: " + ffmpeg::errorString(ret));
            cleanup();
            return;
        }

        {
            ffmpeg::FFmpegVideoTranscodePipeline::Config videoConfig;
            videoConfig.transcodeConfig = &m_config;
            videoConfig.inputVideoStream = inputVideoStream;
            videoConfig.outputFmtCtx = outputFmtCtx;
            videoConfig.timeline = &timeline;

            std::string videoError;
            if (!videoPipeline.initialize(videoConfig, &videoError)) {
                fail(videoError);
                cleanup();
                return;
            }
        }

        if (inputAudioStream && m_config.audioMode != AudioMode::None) {
            ffmpeg::FFmpegAudioPipeline::Config audioConfig;
            audioConfig.mode = m_config.audioMode;
            audioConfig.inputAudioStream = inputAudioStream;
            audioConfig.outputFmtCtx = outputFmtCtx;
            audioConfig.timeline = &timeline;
            audioConfig.audioBitrateKbps = m_config.audioBitrateKbps;

            std::string audioError;
            if (!audioPipeline.initialize(audioConfig, &audioError)) {
                fail(audioError);
                cleanup();
                return;
            }
        }

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

        inputPacket = av_packet_alloc();
        if (!inputPacket) {
            fail("av_packet_alloc input packet failed");
            cleanup();
            return;
        }

        auto onVideoPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
            encodedVideoPacketCount = packetCount;
            lastWrittenVideoOutTimeMs = outTimeMs;

            if (encodedVideoPacketCount == 1 ||
                encodedVideoPacketCount % 25 == 0) {
                emitProgress("transcoding");
            }
        };

        auto onAudioPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
            encodedAudioPacketCount = packetCount;
            lastWrittenAudioOutTimeMs = outTimeMs;

            if (encodedAudioPacketCount == 1 ||
                encodedAudioPacketCount % 25 == 0) {
                emitProgress("transcoding");
            }
        };

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
                std::string videoError;
                const bool ok = videoPipeline.processPacket(
                    inputPacket,
                    &videoError,
                    onVideoPacketWritten
                );

                av_packet_unref(inputPacket);

                if (!ok) {
                    fail(videoError);
                    cleanup();
                    return;
                }
            }
            else if (inputPacket->stream_index == audioStreamIndex &&
                audioPipeline.outputStream()) {
                std::string audioError;
                const bool ok = audioPipeline.processPacket(
                    inputPacket,
                    &audioError,
                    onAudioPacketWritten
                );

                av_packet_unref(inputPacket);

                if (!ok) {
                    fail(audioError);
                    cleanup();
                    return;
                }
            }
            else {
                av_packet_unref(inputPacket);
            }
        }

        if (!m_stopRequested.load()) {
            std::string videoError;
            if (!videoPipeline.flushDecoder(&videoError, onVideoPacketWritten)) {
                fail(videoError);
                cleanup();
                return;
            }
        }

        if (!m_stopRequested.load()) {
            std::string audioError;
            if (!audioPipeline.flush(&audioError, onAudioPacketWritten)) {
                fail(audioError);
                cleanup();
                return;
            }
        }

        if (!m_stopRequested.load()) {
            std::string videoError;
            if (!videoPipeline.flushFilterAndEncoder(&videoError, onVideoPacketWritten)) {
                fail(videoError);
                cleanup();
                return;
            }
        }

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
