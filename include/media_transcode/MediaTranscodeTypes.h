#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace media {

    // 视频编码格式。使用者只指定目标格式，不指定具体编码器实现。
    enum class VideoCodec {
        Copy,
        H264,
        H265,
        MPEG4,
        VP8,
        VP9,
        AV1
    };

    // 视频码控模式。Auto 表示由码率规划层根据业务策略解析。
    enum class VideoRateControlMode {
        Auto,
        CBR,
        VBR,
        CRF,
        CappedVBR
    };

    // 对外暴露的通用编码速度/质量档位。
    // 具体编码器参数由 FFmpeg 编码器适配层根据最终选择到的编码器转换，
    // 例如 NVENC 转为 p1/p4/p7，libx264/libx265 转为 fast/medium/slow。
    enum class VideoEncodeSpeedPreset {
        Auto,
        Fast,
        Balanced,
        Quality
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

    // 码率规划策略。
    // 核心算法只消费这里的规则，不内置分辨率码率梯度等业务经验值。
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

        std::vector<VideoBitrateLadderRule> ladderRules;
        std::vector<VideoCodecBitrateFactor> codecFactors;
        std::vector<VideoBitrateIntentFactor> intentFactors;
        std::vector<VideoContentBitrateFactor> contentFactors;
    };

    // 码率规划输入。0/Auto 表示该项由 policy 决定。
    struct VideoBitrateControlOptions {
        VideoRateControlMode rateControl = VideoRateControlMode::Auto;
        VideoBitrateIntent intent = VideoBitrateIntent::Auto;
        VideoContentHint contentHint = VideoContentHint::Auto;

        // 通用质量值。0 表示自动；数值含义由适配层映射到 crf/cq 等编码器私有参数。
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

    // 视频编码基础控制项。
    // 所有字段均为可选：0、Auto 或空字符串表示使用模块默认值。
    struct VideoEncodeOptions {
        VideoEncodeSpeedPreset speedPreset = VideoEncodeSpeedPreset::Auto;

        // 关键帧间隔，单位：帧。0 表示自动，默认约 2 秒 GOP。
        int gopSize = 0;

        // B 帧数量。当前默认仍为 0，保持低延迟行为。
        int maxBFrames = 0;

        // 编码器私有参数。面向内部适配/专家模式，普通调用方应优先使用 speedPreset。
        std::string preset;
        std::string tune;
        std::string profile;
        std::string level;
    };

    // 音频处理模式
    enum class AudioMode {
        None,
        CopySelected,
        EncodeSelected
    };

    // 音频编码类型，仅在 AudioMode::EncodeSelected 时生效
    enum class AudioCodec {
        AAC,
        OPUS,
        MP3
    };

    struct HardwarePipelineConfig {
        // true：默认自动选择 CPU 占用最低的硬件参与路径。
        // false：显式禁用硬件，强制纯 CPU 解码/滤镜/编码路径。
        bool enabled = true;

        // true：全硬件零拷贝不可用时，允许退到次优硬件混合路径或 CPU 路径。
        // false：零拷贝不可用则初始化失败，用于压测严格零拷贝能力。
        bool allowZeroCopyFallback = true;
    };

    // 进度信息
    struct ProgressInfo {
        int64_t frame = 0;
        int64_t outTimeMs = 0;
        double speed = 0.0;
        std::string raw;
    };

    using ProgressCallback = std::function<void(const ProgressInfo&)>;

    // 单路转码配置
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

        AudioMode audioMode = AudioMode::EncodeSelected;
        AudioCodec audioCodec = AudioCodec::AAC;

        int audioBitrateKbps = 128;

        HardwarePipelineConfig hardware;
    };

} // namespace media
