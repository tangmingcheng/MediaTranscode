#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/core/MediaPort.h"
#include "internal/graph/model/MediaNodeKind.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaNode {
    MediaNodeId id;
    MediaNodeKind kind = MediaNodeKind::Unknown;

    std::string name;
    std::string diagnosticName;

    std::vector<MediaPort> inputPorts;
    std::vector<MediaPort> outputPorts;

    bool required = true;

    const MediaPort* findInputPort(const std::string& portName) const noexcept
    {
        for (const MediaPort& port : inputPorts) {
            if (port.name == portName) {
                return &port;
            }
        }

        return nullptr;
    }

    const MediaPort* findOutputPort(const std::string& portName) const noexcept
    {
        for (const MediaPort& port : outputPorts) {
            if (port.name == portName) {
                return &port;
            }
        }

        return nullptr;
    }

    bool isValid() const noexcept
    {
        return id.isValid() && kind != MediaNodeKind::Unknown && !name.empty();
    }
};

} // namespace media::ffmpeg::graph
