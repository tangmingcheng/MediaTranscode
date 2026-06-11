#pragma once

#include "media_transcode/ITranscoder.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace media {

    class FFmpegTranscoder : public ITranscoder {
    public:
        FFmpegTranscoder();
        ~FFmpegTranscoder() override;

        FFmpegTranscoder(const FFmpegTranscoder&) = delete;
        FFmpegTranscoder& operator=(const FFmpegTranscoder&) = delete;

        bool initialize(const TranscodeConfig& config) override;
        bool start() override;
        void stop() override;

        /*
         * 第一版 URL / 文件转码器暂不支持外部 AVFrame 推入。
         * 实时流优先通过 inputUrl，例如 rtsp://、rtmp://、udp://。
         */
        bool pushFrame(void* frame) override;

        void setProgressCallback(ProgressCallback cb) override;

        /*
         * CLI / 后端服务常用接口。
         * start() 只负责启动后台线程；
         * wait() 用于等待自然转码结束。
         */
        bool wait();
        bool isRunning() const;
        std::string lastError() const;

    private:
        void transcodeThread();
        void setLastError(const std::string& error);
        void clearLastError();

    private:
        TranscodeConfig m_config;

        std::thread m_transcodeThread;
        std::atomic_bool m_running{ false };
        std::atomic_bool m_stopRequested{ false };

        mutable std::mutex m_mutex;
        std::string m_lastError;

        ProgressCallback m_progressCallback;
    };

} // namespace media