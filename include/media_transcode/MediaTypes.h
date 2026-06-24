#pragma once

/**
 * @file MediaTypes.h
 * @brief Shared public enum values used by MediaTranscode public APIs and internals.
 */

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

} // namespace media
