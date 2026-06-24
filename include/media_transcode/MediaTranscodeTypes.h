#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace media {

// 视频编码格式。调用方只描述目标格式，不绑定具体 FFmpeg 编码器实现。
enum class VideoCodec {
    Copy,
    H264,
    H265,
    MPEG4,
    VP8,
    VP9,
    AV1
};

// 视频码控模式。Auto 表示由策略层根据输入和编码器能力自动解析。
enum class VideoRateControlMode {
    Auto,
    CBR,
    VBR,
    CRF,
    CappedVBR
};

// 对外暴露的通用编码速度 preset。
// 对 NVENC / QSV / RKMPP / MediaFoundation 等非 x26x 编码器，由适配层映射到对应 native preset。
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

// 业务侧码率意图，不直接等同编码器参数。
enum class VideoBitrateIntent {
    Auto,
    BandwidthFirst,
    Balanced,
    QualityFirst
};

// 内容类型提示，用于 policy 侧决定码率放大/收缩策略。
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

/**
 * @brief 码率规划策略。
 *
 * 核心算法只消费这里的规则，不内置分辨率码率梯度等业务经验值。
 * 第三方业务可以完全不传 policy，使用默认自动策略；也可以按项目需要下发梯度表。
 */
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

    // 可选的最小码率推导比例。0 表示不自动推导。
    double cbrMinToTargetRatio = 0.0;
    double vbrMinToTargetRatio = 0.0;

    std::vector<VideoBitrateLadderRule> ladderRules;
    std::vector<VideoCodecBitrateFactor> codecFactors;
    std::vector<VideoBitrateIntentFactor> intentFactors;
    std::vector<VideoContentBitrateFactor> contentFactors;
};

/**
 * @brief 用户侧码率输入。
 *
 * 0 / Auto 表示“未指定，由策略层决定”。执行层不直接消费该结构，而是消费规划后的
 * VideoBitratePlan，这样可以避免业务输入和编码器私有参数耦合。
 */
struct VideoBitrateControlOptions {
    VideoRateControlMode rateControl = VideoRateControlMode::Auto;
    VideoBitrateIntent intent = VideoBitrateIntent::Auto;
    VideoContentHint contentHint = VideoContentHint::Auto;

    // 通用质量值。0 表示自动；具体含义由适配层映射到 crf / cq / qp 等编码器参数。
    int quality = 0;

    int targetKbps = 0;
    int minKbps = 0;
    int maxKbps = 0;
    int bufferSizeKbits = 0;
};

// 码率规划结果。FFmpeg 执行层只允许消费该结果，不直接读取用户输入字段。
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

/**
 * @brief 视频编码基础控制项。
 *
 * 所有字段均可选。默认 speedPreset=Medium、maxBFrames=0，偏向稳定和低延迟。
 */
struct VideoEncodeOptions {
    VideoEncodeSpeedPreset speedPreset = VideoEncodeSpeedPreset::Medium;

    // 关键帧间隔，单位：帧。0 表示自动，当前默认约 2 秒 GOP。
    int gopSize = 0;

    // B 帧数量。默认 0，保持较低延迟。
    int maxBFrames = 0;

    // 编码器可选字符串参数。执行层仅在编码器支持时应用。
    std::string tune;
    std::string profile;
    std::string level;
};

// 音频编码类型。Auto 表示保持输入音频编码；只有目标参数变化时才重编码。
enum class AudioCodec {
    Auto,
    AAC,
    OPUS,
    MP3
};

/**
 * @brief 硬件加速策略。
 */
struct HardwarePipelineConfig {
    // true：自动选择 CPU 占用较低的硬件参与路径；false：强制纯 CPU 路径。
    bool enabled = true;

    // true：全硬件零拷贝不可用时允许回退；false：零拷贝不可用即初始化失败。
    bool allowZeroCopyFallback = true;
};

/**
 * @brief 转码进度。
 *
 * frame 是已写出视频包计数；outTimeMs 是当前输出时间戳；speed 为媒体时间 / 真实耗时。
 * raw 是内部阶段字符串，主要用于日志和诊断。
 */
struct ProgressInfo {
    int64_t frame = 0;
    int64_t outTimeMs = 0;
    double speed = 0.0;
    std::string raw;
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

/**
 * @brief 单路文件 / URL 转码配置。
 *
 * 当前公开 API 聚焦单输入单输出转码。未来合屏应新增 MultiInput/Compose 配置，
 * 不建议把合屏参数塞进这个单路配置里。
 */
struct TranscodeConfig {
    std::string inputUrl;
    std::string outputUrl;

    // 0 表示保持输入宽 / 高 / 帧率。width 和 height 建议同时指定。
    int width = 0;
    int height = 0;
    int fps = 0;

    VideoCodec videoCodec = VideoCodec::H264;
    VideoBitrateControlOptions videoBitrate;
    VideoBitrateControlPolicy bitratePolicy;
    VideoEncodeOptions videoEncode;

    // false 表示输出不包含音频；true 表示按目标音频参数自动决定 copy 或 encode。
    bool audioEnabled = true;
    AudioCodec audioCodec = AudioCodec::Auto;

    // 0 表示保持输入音频码率；大于 0 表示显式目标码率。
    int audioBitrateKbps = 0;

    HardwarePipelineConfig hardware;

    /**
     * @brief 创建最小可用配置。
     */
    static TranscodeConfig make(std::string input, std::string output)
    {
        TranscodeConfig config;
        config.inputUrl = std::move(input);
        config.outputUrl = std::move(output);
        return config;
    }

    TranscodeConfig& setInput(std::string value)
    {
        inputUrl = std::move(value);
        return *this;
    }

    TranscodeConfig& setOutput(std::string value)
    {
        outputUrl = std::move(value);
        return *this;
    }

    TranscodeConfig& setVideoCodec(VideoCodec value) noexcept
    {
        videoCodec = value;
        return *this;
    }

    TranscodeConfig& setVideoSize(int outputWidth, int outputHeight) noexcept
    {
        width = outputWidth;
        height = outputHeight;
        return *this;
    }

    TranscodeConfig& keepInputSize() noexcept
    {
        width = 0;
        height = 0;
        return *this;
    }

    TranscodeConfig& setFps(int outputFps) noexcept
    {
        fps = outputFps;
        return *this;
    }

    TranscodeConfig& keepInputFps() noexcept
    {
        fps = 0;
        return *this;
    }

    TranscodeConfig& setVideoBitrate(int targetKbps) noexcept
    {
        videoBitrate.targetKbps = targetKbps;
        return *this;
    }

    TranscodeConfig& setRateControl(VideoRateControlMode mode) noexcept
    {
        videoBitrate.rateControl = mode;
        return *this;
    }

    TranscodeConfig& setQuality(int value) noexcept
    {
        videoBitrate.quality = value;
        return *this;
    }

    TranscodeConfig& setSpeedPreset(VideoEncodeSpeedPreset preset) noexcept
    {
        videoEncode.speedPreset = preset;
        return *this;
    }

    TranscodeConfig& setGopSize(int frames) noexcept
    {
        videoEncode.gopSize = frames;
        return *this;
    }

    TranscodeConfig& setBFrames(int count) noexcept
    {
        videoEncode.maxBFrames = count;
        return *this;
    }

    TranscodeConfig& setAudio(AudioCodec codec = AudioCodec::Auto, int bitrateKbps = 0) noexcept
    {
        audioEnabled = true;
        audioCodec = codec;
        audioBitrateKbps = bitrateKbps;
        return *this;
    }

    TranscodeConfig& disableAudio() noexcept
    {
        audioEnabled = false;
        return *this;
    }

    TranscodeConfig& enableHardware(bool value = true) noexcept
    {
        hardware.enabled = value;
        return *this;
    }

    TranscodeConfig& requireZeroCopy(bool value = true) noexcept
    {
        hardware.enabled = true;
        hardware.allowZeroCopyFallback = !value;
        return *this;
    }

    bool hasResize() const noexcept
    {
        return width > 0 || height > 0;
    }

    bool hasFpsConversion() const noexcept
    {
        return fps > 0;
    }
};

/**
 * @brief 同步转码完成后的摘要。
 */
struct TranscodeReport {
    TranscodeConfig config;
    ProgressInfo lastProgress;
    bool completed = false;
    bool stopped = false;
};

} // namespace media
