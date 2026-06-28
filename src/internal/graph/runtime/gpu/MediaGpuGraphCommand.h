#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGpuGraphCommandKind {
    Unknown,
    Upload,
    Download,
    Map,
    Unmap,
    Kernel,
    Synchronize
};

struct MediaGpuGraphCommand {
    MediaGpuGraphCommandKind kind = MediaGpuGraphCommandKind::Unknown;
    MediaNodeId nodeId = MediaNodeId::invalid();
    MediaHardwareDeviceKind deviceKind = MediaHardwareDeviceKind::Unknown;
    std::string name;
};

struct MediaGpuGraphCommandList {
    std::vector<MediaGpuGraphCommand> commands;

    bool empty() const noexcept
    {
        return commands.empty();
    }
};

} // namespace media::ffmpeg::graph
