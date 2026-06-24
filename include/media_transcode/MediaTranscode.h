#pragma once

/**
 * @file MediaTranscode.h
 * @brief Public API for using MediaTranscode as a C++ library.
 *
 * This header is intentionally small: it exposes only stable public types and
 * the current local-file transcode entry point. FFmpeg classes, pipeline stages,
 * queues and hardware planners are implementation details and should not be
 * included by third-party applications.
 */

#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>
#include <string>

namespace media {

/**
 * @brief Output video codec requested by the caller.
 *
 * The library selects the concrete FFmpeg encoder internally according to
 * platform capabilities and hardware settings.
 */
enum class OutputVideoCodec {
    H264,
    H265,
    MPEG4,
    VP8,
    VP9,
    AV1
};

/**
 * @brief Video rate-control mode.
 *
 * Auto lets the library choose a suitable mode from the supplied bitrate and
 * quality values. CBR/VBR/CRF/CappedVBR are mapped to encoder-specific options
 * when the selected encoder supports them.
 */
enum class VideoRcMode {
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
 * @brief Output audio codec.
 *
 * Auto keeps/copies the input audio when possible and only re-encodes when the
 * output container or explicit audio bitrate requires it.
 */
enum class OutputAudioCodec {
    Auto,
    AAC,
    OPUS,
    MP3
};

/**
 * @brief Progress emitted during local file transcoding.
 */
struct TranscodeProgress {
    int64_t frame = 0;
    int64_t outTimeMs = 0;
    double speed = 0.0;
    std::string stage;
};

using TranscodeProgressCallback = std::function<void(const TranscodeProgress&)>;

/**
 * @brief Unified local-file transcode parameters.
 *
 * All fields are plain data so third-party callers can construct, serialize or
 * modify the configuration without learning a builder API. A value of 0 means
 * "use input/default" for size, fps, bitrate, quality and GOP-related fields.
 */
struct LocalTranscodeConfig {
    /** Input media path or URL accepted by FFmpeg. */
    std::string inputUrl;

    /** Output media path or URL accepted by FFmpeg. */
    std::string outputUrl;

    /** Output width. 0 keeps the input width. */
    int width = 0;

    /** Output height. 0 keeps the input height. */
    int height = 0;

    /** Output frames per second. 0 keeps the input frame rate. */
    int fps = 0;

    /** Requested output video codec. */
    OutputVideoCodec videoCodec = OutputVideoCodec::H264;

    /** Target video bitrate in kbps. 0 lets the library decide. */
    int videoBitrateKbps = 0;

    /** Minimum video bitrate in kbps. 0 means unspecified. */
    int minVideoBitrateKbps = 0;

    /** Maximum video bitrate in kbps. 0 means unspecified. */
    int maxVideoBitrateKbps = 0;

    /** Rate-control buffer size in kbits. 0 means unspecified. */
    int videoBufferSizeKbits = 0;

    /** Video rate-control mode. */
    VideoRcMode rcMode = VideoRcMode::Auto;

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
    OutputAudioCodec audioCodec = OutputAudioCodec::Auto;

    /** Target audio bitrate in kbps. 0 keeps/copies input bitrate when possible. */
    int audioBitrateKbps = 0;

    /** true removes audio from the output. */
    bool noAudio = false;
};

/**
 * @brief Final result summary for a local transcode job.
 */
struct LocalTranscodeReport {
    LocalTranscodeConfig config;
    TranscodeProgress lastProgress;
};

/**
 * @brief Transcode one local file or FFmpeg-readable URL synchronously.
 *
 * This is currently the only supported public operation. The function returns
 * after the job finishes or fails. For progress updates, pass a callback; pass
 * an empty callback when progress is not needed.
 */
[[nodiscard]] Result<LocalTranscodeReport> transcodeLocalFile(
    const LocalTranscodeConfig& config,
    TranscodeProgressCallback progressCallback = {}
);

} // namespace media
