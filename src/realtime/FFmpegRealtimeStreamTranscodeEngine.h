#pragma once

#include "internal/TranscodeTypes.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace media {

namespace ffmpeg {
class FFmpegRealtimeInputSource;
}

/**
 * @brief Internal runtime state for the P1 realtime stream transcode core.
 */
enum class RealtimeStreamState {
    Idle,
    Initialized,
    Running,
    StopRequested,
    Stopped,
    Failed
};

/**
 * @brief RTP output options used by the realtime core.
 */
struct RealtimeRtpOutputConfig {
    std::string host;
    int rtpPort = 0;
    int rtcpPort = 0;
    int localRtpPort = 0;
    int localRtcpPort = 0;
    int packetSize = 1200;
    std::string sdpOutputPath;
};

/**
 * @brief Internal configuration for validating the realtime RTP transcode core.
 *
 * This is deliberately separate from LocalVideoTranscodeConfig. Realtime stream
 * behavior has different lifetime, timeout and output semantics from local file
 * transcoding, so this object should not be exposed as stable public API yet.
 *
 * Video encoding options intentionally reuse the same internal structures as the
 * local-file pipeline so realtime and local transcoding do not diverge while P1
 * evolves.
 */
struct RealtimeCoreConfig {
    RealtimeCoreConfig()
    {
        videoBitrate.rateControl = VideoRateControlMode::CBR;
        videoEncode.speedPreset = VideoSpeedPreset::Veryfast;
        videoEncode.maxBFrames = 0;
        hardware.enabled = true;
        hardware.allowZeroCopyFallback = true;
    }

    std::string inputUrl;
    std::string inputFormatHint;

    RealtimeRtpOutputConfig rtpOutput;

    int width = 0;
    int height = 0;
    int fps = 0;

    VideoCodec videoCodec = VideoCodec::H264;
    VideoBitrateControlOptions videoBitrate;
    VideoBitrateControlPolicy bitratePolicy;
    VideoEncodeOptions videoEncode;
    HardwarePipelineConfig hardware;

    /** Reserved for a later realtime audio phase. P1 realtime output is video-only. */
    bool audioEnabled = false;

    int openTimeoutMs = 5000;
    int readTimeoutMs = 5000;
    int analyzeDurationUs = 500000;
    int probeSizeBytes = 512 * 1024;

    bool lowLatency = true;
};

/**
 * @brief Lightweight counters prepared for realtime health diagnostics.
 */
struct RealtimeCoreStats {
    int64_t inputPacketCount = 0;
    int64_t inputVideoPacketCount = 0;
    int64_t decodedVideoFrameCount = 0;
    int64_t encodedVideoPacketCount = 0;
    /** Encoded AVPackets accepted by the RTP muxer. */
    int64_t muxedVideoPacketCount = 0;
    int64_t lastInputTimeMs = 0;
    int64_t lastOutputTimeMs = 0;
};

/**
 * @brief Internal engine for P1 single-stream realtime video-to-RTP transcoding.
 */
class FFmpegRealtimeStreamTranscodeEngine {
public:
    FFmpegRealtimeStreamTranscodeEngine();
    ~FFmpegRealtimeStreamTranscodeEngine();

    FFmpegRealtimeStreamTranscodeEngine(const FFmpegRealtimeStreamTranscodeEngine&) = delete;
    FFmpegRealtimeStreamTranscodeEngine& operator=(const FFmpegRealtimeStreamTranscodeEngine&) = delete;

    Status initialize(const RealtimeCoreConfig& config);

    /** Start the realtime core on an owned worker thread. */
    Status start();

    /** Run the realtime core on the current thread. */
    Status run();

    /** Request the running core to stop. */
    void requestStop();

    /** Join the worker thread if start() was used. */
    Status wait();

    bool isRunning() const;
    bool stopRequested() const;

    RealtimeStreamState state() const;
    RealtimeCoreStats stats() const;
    ErrorInfo lastError() const;

    void setProgressCallback(ProgressCallback cb);

private:
    Status validateConfig(const RealtimeCoreConfig& config) const;
    Status runLoop();
    void workerThread();

    void resetRuntimeState();
    void clearLastError();
    void setLastError(ErrorInfo error);
    void setState(RealtimeStreamState state);
    void emitProgress(const std::string& stage);

private:
    mutable std::mutex m_mutex;

    RealtimeCoreConfig m_config;
    RealtimeCoreStats m_stats;
    ErrorInfo m_lastError;
    RealtimeStreamState m_state = RealtimeStreamState::Idle;
    ProgressCallback m_progressCallback;
    std::unique_ptr<ffmpeg::FFmpegRealtimeInputSource> m_inputSource;

    std::thread m_workerThread;
    std::atomic_bool m_running{ false };
    std::atomic_bool m_stopRequested{ false };
};

} // namespace media
