#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"

#include <utility>

namespace media::ffmpeg::graph {

bool MediaGraphClusterTopology::addNode(MediaGraphClusterNode node)
{
    if (!node.address.valid()) {
        return false;
    }

    const std::string id = node.address.nodeId;
    m_nodes[id] = std::move(node);
    return true;
}

bool MediaGraphClusterTopology::removeNode(const std::string& nodeId)
{
    return m_nodes.erase(nodeId) > 0;
}

MediaGraphClusterNode* MediaGraphClusterTopology::findNode(const std::string& nodeId)
{
    auto it = m_nodes.find(nodeId);
    return it == m_nodes.end() ? nullptr : &it->second;
}

const MediaGraphClusterNode* MediaGraphClusterTopology::findNode(const std::string& nodeId) const
{
    auto it = m_nodes.find(nodeId);
    return it == m_nodes.end() ? nullptr : &it->second;
}

std::vector<MediaGraphClusterNode> MediaGraphClusterTopology::nodes() const
{
    std::vector<MediaGraphClusterNode> result;
    result.reserve(m_nodes.size());
    for (const auto& item : m_nodes) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<MediaGraphClusterNode> MediaGraphClusterTopology::workers() const
{
    std::vector<MediaGraphClusterNode> result;
    for (const auto& item : m_nodes) {
        if (item.second.available && item.second.role == MediaGraphClusterNodeRole::Worker) {
            result.push_back(item.second);
        }
    }
    return result;
}

void MediaGraphClusterTopology::clear()
{
    m_nodes.clear();
}

std::size_t MediaGraphClusterTopology::size() const noexcept
{
    return m_nodes.size();
}

} // namespace media::ffmpeg::graph
