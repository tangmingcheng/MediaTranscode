#include "internal/graph/runtime/distributed/MediaGraphDeploymentPlan.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaGraphDeploymentPlan::assign(MediaNodeId graphNodeId, MediaGraphNodeAddress address)
{
    if (!graphNodeId || !address.valid()) {
        return;
    }

    MediaGraphDeploymentAssignment assignment;
    assignment.graphNodeId = graphNodeId;
    assignment.clusterNodeId = address.nodeId;
    assignment.address = std::move(address);
    m_assignments[graphNodeId.value] = std::move(assignment);
}

const MediaGraphDeploymentAssignment* MediaGraphDeploymentPlan::find(MediaNodeId graphNodeId) const
{
    const auto it = m_assignments.find(graphNodeId.value);
    return it == m_assignments.end() ? nullptr : &it->second;
}

std::vector<MediaGraphDeploymentAssignment> MediaGraphDeploymentPlan::assignments() const
{
    std::vector<MediaGraphDeploymentAssignment> result;
    result.reserve(m_assignments.size());
    for (const auto& item : m_assignments) {
        result.push_back(item.second);
    }
    return result;
}

void MediaGraphDeploymentPlan::clear()
{
    m_assignments.clear();
}

bool MediaGraphDeploymentPlan::empty() const noexcept
{
    return m_assignments.empty();
}

} // namespace media::ffmpeg::graph
