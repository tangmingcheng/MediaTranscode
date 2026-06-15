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

    // 内部执行管线类型。应用层通常只设置 HardwarePipelineConfig::requireZeroCopy。
    enum class VideoFramePipeline {
        Cpu,
        Hardware
    };

    // FFmpeg 硬件设备类型抽象。该枚举属于后端 planner 内部能力表达，应用层不需要指定。
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
        // 使用方唯一需要表达的硬件意图：是否要求硬解码 + 硬件滤镜 + 硬件编码零拷贝闭环。
        // true：后端 planner 必须找到严格零拷贝方案，否则初始化失败。
        // false：使用普通 CPU frame 转码路径。
        bool requireZeroCopy = false;

        // 以下字段仅作为 planner / pipeline 内部执行状态保留，不应暴露给 CLI 或业务层配置。
        VideoFramePipeline videoFramePipeline = VideoFramePipeline::Cpu;
        HardwareDeviceType deviceType = HardwareDeviceType::Auto;
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
