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

    // 视频码控模式。Auto 表示由具体编码器适配层选择最安全的默认策略。
    enum class VideoRateControlMode {
        Auto,
        CBR,
        VBR
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

    // 视频编码基础控制项。
    // 所有字段均为可选：0、Auto 或空字符串表示使用模块默认值。
    struct VideoEncodeOptions {
        VideoRateControlMode rateControl = VideoRateControlMode::Auto;
        VideoEncodeSpeedPreset speedPreset = VideoEncodeSpeedPreset::Auto;

        // 关键帧间隔，单位：帧。0 表示自动，默认约 2 秒 GOP。
        int gopSize = 0;

        // B 帧数量。当前默认仍为 0，保持低延迟行为。
        int maxBFrames = 0;

        // VBR/CBR 辅助参数。0 表示不显式设置。
        int maxBitrateKbps = 0;
        int bufferSizeKbps = 0;

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
        VideoEncodeOptions videoEncode;

        AudioMode audioMode = AudioMode::EncodeSelected;
        AudioCodec audioCodec = AudioCodec::AAC;

        int audioBitrateKbps = 128;
        int videoBitrateKbps = 3000;

        HardwarePipelineConfig hardware;
    };

} // namespace media
