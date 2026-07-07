#pragma once

namespace media::ffmpeg::graph {

enum class MediaZeroCopyMode {
    Disabled,
    Prefer,
    Required
};

enum class MediaInteropKind {
    None,
    AVFrameRef,
    DRMPrime,
    DmaBuf,
    CUDA,
    D3D11,
    VAAPI,
    RKMPP
};

struct MediaZeroCopyPolicy {
    MediaZeroCopyMode mode = MediaZeroCopyMode::Prefer;
    MediaInteropKind preferredInterop = MediaInteropKind::AVFrameRef;

    bool allowHardwareMapping = true;
    bool allowHardwareDownload = true;
    bool allowHardwareUpload = true;
    bool allowSoftwareTransfer = true;

    constexpr bool enabled() const noexcept
    {
        return mode != MediaZeroCopyMode::Disabled;
    }

    constexpr bool required() const noexcept
    {
        return mode == MediaZeroCopyMode::Required;
    }
};

} // namespace media::ffmpeg::graph
