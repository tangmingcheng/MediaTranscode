#pragma once

#include "internal/graph/runtime/distributed/MediaGraphNodeAddress.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphClusterNodeRole {
    Unknown,
    Coordinator,
    Worker,
    Edge,
    Storage
};

struct MediaGraphClusterNode {
    MediaGraphNodeAddress address;
    MediaGraphClusterNodeRole role = MediaGraphClusterNodeRole::Unknown;
    bool available = true;
    int weight = 1;
};

class MediaGraphClusterTopology final {
public:
    bool addNode(MediaGraphClusterNode node);
    bool removeNode(const std::string& nodeId);
    MediaGraphClusterNode* findNode(const std::string& nodeId);
    const MediaGraphClusterNode* findNode(const std::string& nodeId) const;

    std::vector<MediaGraphClusterNode> nodes() const;
    std::vector<MediaGraphClusterNode> workers() const;
    void clear();
    std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, MediaGraphClusterNode> m_nodes;
};

} // namespace media::ffmpeg::graph
