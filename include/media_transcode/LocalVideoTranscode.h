#pragma once

/**
 * @file LocalVideoTranscode.h
 * @brief Public API for the local video file transcode capability.
 */

#include "media_transcode/MediaTypes.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace media {

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

/**
 * @brief Opaque local video transcode job handle.
 *
 * The complete job definition is private to the implementation. Callers own the
 * returned handle and operate on it through the local video transcode functions
 * below. This keeps the public API close to an FFmpeg-style opaque handle model.
 */
struct LocalVideoTranscodeJob;

using LocalVideoTranscodeJobHandle = std::shared_ptr<LocalVideoTranscodeJob>;

/**
 * @brief Start local video transcoding asynchronously.
 *
 * The returned job owns the running work. Call waitLocalVideoTranscode() to join
 * and obtain the final report, or stopLocalVideoTranscode() to request early
 * termination.
 */
[[nodiscard]] Result<LocalVideoTranscodeJobHandle> startLocalVideoTranscodeAsync(
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

/**
 * @brief Request a running local video transcode job to stop.
 */
[[nodiscard]] Result<void> stopLocalVideoTranscode(
    const LocalVideoTranscodeJobHandle& job
);

/**
 * @brief Wait until a local video transcode job finishes or fails.
 */
[[nodiscard]] Result<LocalVideoTranscodeReport> waitLocalVideoTranscode(
    const LocalVideoTranscodeJobHandle& job
);

/**
 * @brief Return whether the local video transcode job is currently running.
 */
[[nodiscard]] bool isLocalVideoTranscodeRunning(
    const LocalVideoTranscodeJobHandle& job
);

/**
 * @brief Return the last known error for the local video transcode job.
 */
[[nodiscard]] ErrorInfo getLocalVideoTranscodeLastError(
    const LocalVideoTranscodeJobHandle& job
);

/**
 * @brief Return the last known progress for the local video transcode job.
 */
[[nodiscard]] LocalVideoTranscodeProgress getLocalVideoTranscodeLastProgress(
    const LocalVideoTranscodeJobHandle& job
);

} // namespace media
