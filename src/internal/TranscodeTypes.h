#pragma once

#include "media_transcode/MediaTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace media {

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
    VideoSpeedPreset speedPreset = VideoSpeedPreset::Medium;
    int gopSize = 0;
    int maxBFrames = 0;
    std::string tune;
    std::string profile;
    std::string level;
};

struct HardwarePipelineConfig {
    bool enabled = true;
    bool allowZeroCopyFallback = true;
};

struct ProgressInfo {
    int64_t frame = 0;
    int64_t outTimeMs = 0;
    double speed = 0.0;
    std::string raw;
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

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

} // namespace media
