#pragma once

#include <string>

namespace media::ffmpeg::graph {

struct MediaGraphNodeAddress {
    std::string nodeId;
    std::string host;
    int port = 0;
    std::string zone;

    bool valid() const noexcept
    {
        return !nodeId.empty() && !host.empty() && port > 0;
    }
};

} // namespace media::ffmpeg::graph
