#pragma once

namespace media::ffmpeg {

    // Internal execution pipeline type. Public callers do not choose this directly;
    // FFmpegPipelinePlanner decides the runtime path.
    enum class VideoFramePipeline {
        Cpu,
        Hardware
    };

    // Internal FFmpeg hardware device abstraction. Public callers should not select
    // a concrete backend such as D3D11VA, CUDA, VAAPI, or QSV.
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

} // namespace media::ffmpeg
