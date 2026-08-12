#pragma once

#include <string>

namespace media::ffmpeg::graph {

enum class MediaHardwareDeviceKind {
    Unknown,
    None,
    D3D11VA,
    QSV,
    CUDA,
    VAAPI,
    DRMPrime,
    RKMPP,
    VideoToolbox,
    MediaCodec
};

enum class MediaHardwareFrameKind {
    Unknown,
    Software,
    Hardware,
    HardwareMapped
};

enum class MediaHardwareTransferDirection {
    Unknown,
    None,
    Upload,
    Download,
    Map,
    Unmap
};

struct MediaHardwareDescriptor {
    MediaHardwareDeviceKind deviceKind = MediaHardwareDeviceKind::Unknown;
    MediaHardwareFrameKind frameKind = MediaHardwareFrameKind::Unknown;
    MediaHardwareTransferDirection transferDirection = MediaHardwareTransferDirection::Unknown;

    std::string deviceName;
    std::string pixelFormat;
    std::string surfacePixelFormat;
    std::string framesContextName;

    bool zeroCopyPreferred = true;
    bool requiresHardwareDeviceContext = false;
    bool requiresHardwareFramesContext = false;

    constexpr bool isHardwareBacked() const noexcept
    {
        return frameKind == MediaHardwareFrameKind::Hardware ||
               frameKind == MediaHardwareFrameKind::HardwareMapped;
    }
};

} // namespace media::ffmpeg::graph
