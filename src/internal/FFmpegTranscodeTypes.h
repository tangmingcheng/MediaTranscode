#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace media::ffmpeg {

// Internal video codec model used by the FFmpeg pipeline. Public API maps to it
// in MediaTranscode.cpp so FFmpeg-specific compatibility values do not leak out.
enum class VideoCodec {
    Copy,
    H264,
    H265,
    MPEG4,
    VP8,
    VP9,
    AV1
};

enum class VideoRateControlMode {
    Auto,
    CBR,
    VBR,
    CRF,
    CappedVBR
};

enum class VideoEncodeSpeedPreset {
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

enum class VideoBitrateIntent {
    Auto,
    BandwidthFirst,
    Balanced,
    QualityFirst
};

enum class VideoContentHint {
    Auto,
    Camera,
    Screen,
    Animation,
    Film,
    Sports
};

struct VideoBitrateLadderRule {
    int maxPixels = 0;
    int targetKbps = 0;
};

struct VideoCodecBitrateFactor {
    VideoCodec codec = VideoCodec::H264;
    double factor = 1.0;
};

struct VideoBitrateIntentFactor {
    VideoBitrateIntent intent = VideoBitrateIntent::Balanced;
    double factor = 1.0;
};

struct VideoContentBitrateFactor {
    VideoContentHint content = VideoContentHint::Auto;
    double factor = 1.0;
};

struct VideoBitrateControlPolicy {
    VideoRateControlMode defaultRateControl = VideoRateControlMode::Auto;

    int fallbackTargetKbps = 0;
    int minimumTargetKbps = 0;
    int maximumTargetKbps = 0;

    int defaultQuality = 0;
    int minimumQuality = 0;
    int maximumQuality = 0;

    double referenceFps = 0.0;
    double minimumFpsFactor = 0.0;
    double maximumFpsFactor = 0.0;

    double cbrPeakMultiplier = 0.0;
    double vbrPeakMultiplier = 0.0;
    double cbrBufferSeconds = 0.0;
    double vbrBufferSeconds = 0.0;

    double cbrMinToTargetRatio = 0.0;
    double vbrMinToTargetRatio = 0.0;

    std::vector<VideoBitrateLadderRule> ladderRules;
    std::vector<VideoCodecBitrateFactor> codecFactors;
    std::vector<VideoBitrateIntentFactor> intentFactors;
    std::vector<VideoContentBitrateFactor> contentFactors;
};

struct VideoBitrateControlOptions {
    VideoRateControlMode rateControl = VideoRateControlMode::Auto;
    VideoBitrateIntent intent = VideoBitrateIntent::Auto;
    VideoContentHint contentHint = VideoContentHint::Auto;

    int quality = 0;

    int targetKbps = 0;
    int minKbps = 0;
    int maxKbps = 0;
    int bufferSizeKbits = 0;
};

struct VideoBitratePlan {
    VideoRateControlMode rateControl = VideoRateControlMode::Auto;

    int quality = 0;
    int targetKbps = 0;
    int minKbps = 0;
    int maxKbps = 0;
    int bufferSizeKbits = 0;

    bool userQualityApplied = false;
    bool userTargetApplied = false;
    bool userMinApplied = false;
    bool userMaxApplied = false;
    bool userBufferApplied = false;

    std::vector<std::string> diagnostics;
};

struct VideoEncodeOptions {
    VideoEncodeSpeedPreset speedPreset = VideoEncodeSpeedPreset::Medium;
    int gopSize = 0;
    int maxBFrames = 0;
    std::string tune;
    std::string profile;
    std::string level;
};

enum class AudioCodec {
    Auto,
    AAC,
    OPUS,
    MP3
};

struct HardwarePipelineConfig {
    bool enabled = true;
    bool allowZeroCopyFallback = true;
};

struct TranscodeConfig {
    std::string inputUrl;
    std::string outputUrl;

    int width = 0;
    int height = 0;
    int fps = 0;

    VideoCodec videoCodec = VideoCodec::H264;
    VideoBitrateControlOptions videoBitrate;
    VideoBitrateControlPolicy bitratePolicy;
    VideoEncodeOptions videoEncode;

    bool audioEnabled = true;
    AudioCodec audioCodec = AudioCodec::Auto;
    int audioBitrateKbps = 0;

    HardwarePipelineConfig hardware;
};

} // namespace media::ffmpeg
