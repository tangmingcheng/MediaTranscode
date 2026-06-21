#pragma once

namespace media::ffmpeg {

    // Internal execution mode selected by FFmpegPipelinePlanner.
    // Public callers do not choose this directly.
    enum class VideoExecutionMode {
        Cpu,

        // Hardware decode -> hardware filter/direct hardware frame -> hardware encode.
        // This is the lowest-CPU path and must not require hwdownload.
        HardwareZeroCopy,

        // Hardware decode -> hwdownload -> software filter -> hardware encode.
        HardwareDecodeSoftwareFilterHardwareEncode,

        // Hardware decode -> hwdownload -> software filter -> generic encoder.
        // The generic encoder is usually software, but may still be hardware when it is
        // the only available encoder for the requested codec.
        HardwareDecodeSoftwareFilterGenericEncode
    };

    // Internal execution pipeline type. Public callers do not choose this directly;
    // FFmpegPipelinePlanner decides the runtime path.
    enum class VideoFramePipeline {
        Cpu,
        Hardware
    };

    // Internal FFmpeg hardware device abstraction. Public callers should not select
    // a concrete backend such as D3D11VA, CUDA, VAAPI, QSV, or RKMPP.
    enum class HardwareDeviceType {
        None,
        Auto,
        D3D11VA,
        CUDA,
        QSV,
        VAAPI,
        DRM,
        RKMPP,
        VideoToolbox
    };

} // namespace media::ffmpeg
