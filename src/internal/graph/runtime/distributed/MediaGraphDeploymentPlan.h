#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/distributed/MediaGraphNodeAddress.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphDeploymentAssignment {
    MediaNodeId graphNodeId = MediaNodeId::invalid();
    std::string clusterNodeId;
    MediaGraphNodeAddress address;
};

class MediaGraphDeploymentPlan final {
public:
    void assign(MediaNodeId graphNodeId, MediaGraphNodeAddress address);
    const MediaGraphDeploymentAssignment* find(MediaNodeId graphNodeId) const;
    std::vector<MediaGraphDeploymentAssignment> assignments() const;
    void clear();
    bool empty() const noexcept;

private:
    std::unordered_map<uint32_t, MediaGraphDeploymentAssignment> m_assignments;
};

} // namespace media::ffmpeg::graph
