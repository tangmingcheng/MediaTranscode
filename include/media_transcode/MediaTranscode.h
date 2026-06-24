#pragma once

/**
 * @file MediaTranscode.h
 * @brief Public capability-oriented API for using MediaTranscode as a C++ library.
 *
 * This header intentionally exposes stable library capabilities and shared enum
 * values used by both public API code and internal FFmpeg-backed execution code.
 * Internal execution structs, FFmpeg classes, queues, pipeline stages and hardware
 * planners remain implementation details.
 */

#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace media {

/**
 * @brief Video codec identifier shared by public API and internal implementation.
 *
 * The library maps this logical codec value to a concrete FFmpeg encoder at
 * runtime according to platform capabilities and hardware settings. Copy is a
 * generic pass-through value for future capabilities; local video transcoding
 * currently rejects it because this capability always produces encoded output.
 */
enum class VideoCodec {
    Copy,
    H264,
    H265,
    MPEG4,
    VP8,
    VP9,
    AV1
};

/**
 * @brief Video rate-control mode shared by public API and internal implementation.
 *
 * Auto lets the library choose a suitable mode from the supplied bitrate and
 * quality values. CBR/VBR/CRF/CappedVBR are mapped to encoder-specific options
 * when the selected encoder supports them.
 */
enum class VideoRateControlMode {
    Auto,
    CBR,
    VBR,
    CRF,
    CappedVBR
};

/**
 * @brief Encoder speed / compression preset.
 *
 * Faster presets use less CPU/GPU time and usually produce larger files.
 * Slower presets can improve compression efficiency. Hardware encoders map this
 * value to their nearest native preset when possible.
 */
enum class VideoSpeedPreset {
    Ultrafast,
    Superfast,
    Veryfast,
    Faster,
    Fast,
    Medium,
    Slow,
    Slower,
    Veryslow,
    Placebo
};

/**
 * @brief Audio codec identifier shared by public API and internal implementation.
 *
 * Auto keeps/copies the input audio when possible and only re-encodes when the
 * output container or explicit audio bitrate requires it.
 */
enum class AudioCodec {
    Auto,
    AAC,
    OPUS,
    MP3
};

/**
 * @brief Progress emitted during local video transcoding.
 */
struct LocalVideoTranscodeProgress {
    int64_t frame = 0;
    int64_t outTimeMs = 0;
    double speed = 0.0;
    std::string stage;
};

using LocalVideoTranscodeProgressCallback =
    std::function<void(const LocalVideoTranscodeProgress&)>;

/**
 * @brief Unified configuration for local video file transcoding.
 *
 * All transcode parameters are configured here. The public API intentionally
 * avoids exposing separate setter methods so callers can construct, serialize,
 * validate or persist one plain configuration object.
 *
 * A value of 0 means "use input/default" for size, fps, bitrate, quality and
 * GOP-related fields.
 */
struct LocalVideoTranscodeConfig {
    /** Local input video file path. Network inputs are not part of this API. */
    std::string inputPath;

    /** Local output video file path. */
    std::string outputPath;

    /** Output width. 0 keeps the input width. */
    int width = 0;

    /** Output height. 0 keeps the input height. */
    int height = 0;

    /** Output frames per second. 0 keeps the input frame rate. */
    int fps = 0;

    /** Requested output video codec. VideoCodec::Copy is not supported by this capability. */
    VideoCodec videoCodec = VideoCodec::H264;

    /** Target video bitrate in kbps. 0 lets the library decide. */
    int videoBitrateKbps = 0;

    /** Minimum video bitrate in kbps. 0 means unspecified. */
    int minVideoBitrateKbps = 0;

    /** Maximum video bitrate in kbps. 0 means unspecified. */
    int maxVideoBitrateKbps = 0;

    /** Rate-control buffer size in kbits. 0 means unspecified. */
    int videoBufferSizeKbits = 0;

    /** Video rate-control mode. */
    VideoRateControlMode rcMode = VideoRateControlMode::Auto;

    /** Generic quality value. 0 means automatic; mapping depends on encoder. */
    int quality = 0;

    /** Encoder speed preset. */
    VideoSpeedPreset speed = VideoSpeedPreset::Medium;

    /** GOP size in frames. 0 means automatic. */
    int gopSize = 0;

    /** Maximum B-frame count. 0 keeps low-latency behavior. */
    int maxBFrames = 0;

    /** Optional encoder tune string, applied only when supported. */
    std::string tune;

    /** Optional encoder profile string, applied only when supported. */
    std::string profile;

    /** Optional encoder level string, applied only when supported. */
    std::string level;

    /** true disables hardware decode/filter/encode participation. */
    bool disableHardware = false;

    /** Requested output audio codec. */
    AudioCodec audioCodec = AudioCodec::Auto;

    /** Target audio bitrate in kbps. 0 keeps/copies input bitrate when possible. */
    int audioBitrateKbps = 0;

    /** true removes audio from the output. */
    bool noAudio = false;
};

/**
 * @brief Final result summary for a local video transcode job.
 */
struct LocalVideoTranscodeReport {
    LocalVideoTranscodeConfig config;
    LocalVideoTranscodeProgress lastProgress;
    bool completed = false;
    bool stopped = false;
};

class LocalVideoTranscodeTask final {
public:
    ~LocalVideoTranscodeTask();

    LocalVideoTranscodeTask(const LocalVideoTranscodeTask&) = delete;
    LocalVideoTranscodeTask& operator=(const LocalVideoTranscodeTask&) = delete;

    LocalVideoTranscodeTask(LocalVideoTranscodeTask&&) noexcept;
    LocalVideoTranscodeTask& operator=(LocalVideoTranscodeTask&&) noexcept;

    /**
     * @brief Request the local transcode job to stop and wait for the worker thread to exit.
     */
    void stop();

    /**
     * @brief Wait until the job finishes or fails and return the final report.
     */
    [[nodiscard]] Result<LocalVideoTranscodeReport> wait();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] ErrorInfo lastError() const;
    [[nodiscard]] LocalVideoTranscodeProgress lastProgress() const;

private:
    struct Impl;

    explicit LocalVideoTranscodeTask(std::shared_ptr<Impl> impl);

private:
    std::shared_ptr<Impl> m_impl;

    friend Result<std::shared_ptr<LocalVideoTranscodeTask>> startLocalVideoTranscodeAsync(
        const LocalVideoTranscodeConfig& config,
        LocalVideoTranscodeProgressCallback progressCallback
    );
};

/**
 * @brief Start local video transcoding asynchronously.
 *
 * The returned task owns the running job. Call wait() to join and obtain the
 * final report, or stop() to request early termination.
 */
[[nodiscard]] Result<std::shared_ptr<LocalVideoTranscodeTask>> startLocalVideoTranscodeAsync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback = {}
);

/**
 * @brief Start local video transcoding synchronously and return when the job ends.
 */
[[nodiscard]] Result<LocalVideoTranscodeReport> startLocalVideoTranscodeSync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback = {}
);

} // namespace media
