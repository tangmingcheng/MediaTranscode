#pragma once
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

    // 视频帧管线类型。
    // Cpu：保持现有软件帧管线。
    // Hardware：后续真正的硬件帧路径入口：硬件解码帧 -> 硬件滤镜 -> 硬件编码帧。
    enum class VideoFramePipeline {
        Cpu,
        Hardware
    };

    // FFmpeg 硬件设备类型抽象。
    // 这里不绑定具体编码器顺序，避免把平台策略散落到 FFmpegUtils.cpp。
    enum class HardwareDeviceType {
        None,
        Auto,
        D3D11VA,
        CUDA,
        QSV,
        VAAPI,
        DRM,
        VideoToolbox
    };

    struct HardwarePipelineConfig {
        VideoFramePipeline videoFramePipeline = VideoFramePipeline::Cpu;
        HardwareDeviceType deviceType = HardwareDeviceType::Auto;

        // true：硬件路径未完全可用时允许回退到现有 CPU frame 管线。
        // false：硬件路径初始化失败则直接报错，便于压测硬件路径。
        bool allowSoftwareFallback = true;
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
        AudioMode audioMode = AudioMode::EncodeSelected;
        AudioCodec audioCodec = AudioCodec::AAC;

        int audioBitrateKbps = 128;
        int videoBitrateKbps = 3000;

        HardwarePipelineConfig hardware;
    };

} // namespace media
