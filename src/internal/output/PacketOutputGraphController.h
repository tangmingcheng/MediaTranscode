#pragma once

#include "internal/output/PacketOutputGraph.h"
#include "internal/output/PacketOutputNode.h"
#include "media_transcode/Result.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace media::ffmpeg {

class PacketOutputGraphController final {
public:
    PacketOutputGraphController() = default;
    ~PacketOutputGraphController() = default;

    PacketOutputGraphController(const PacketOutputGraphController&) = delete;
    PacketOutputGraphController& operator=(const PacketOutputGraphController&) = delete;

    PacketOutputGraphController(PacketOutputGraphController&&) = delete;
    PacketOutputGraphController& operator=(PacketOutputGraphController&&) = delete;

    void reset()
    {
        m_graph.clear();
        m_ownedNodes.clear();
    }

    Status attachExternalNode(PacketOutputNode* node)
    {
        return m_graph.attachNode(node);
    }

    Status detachExternalNode(PacketOutputNode* node)
    {
        return m_graph.detachNode(node);
    }

    Status attachOwnedNode(std::unique_ptr<PacketOutputNode> node)
    {
        if (!node) {
            return Status::failure(ErrorInfo::invalidArgument(
                "PacketOutputGraphController attachOwnedNode failed: node is null"));
        }

        PacketOutputNode* raw = node.get();
        Status status = m_graph.attachNode(raw);
        if (!status) {
            return status;
        }

        m_ownedNodes.push_back(std::move(node));
        return Status::success();
    }

    template <typename Node, typename... Args>
    Node* createOwnedNode(Args&&... args)
    {
        static_assert(std::is_base_of_v<PacketOutputNode, Node>,
                      "Node must derive from PacketOutputNode");

        auto node = std::make_unique<Node>(std::forward<Args>(args)...);
        Node* raw = node.get();

        Status status = attachOwnedNode(std::move(node));
        if (!status) {
            return nullptr;
        }

        return raw;
    }

    PacketOutputNode* rootNode()
    {
        return &m_graph;
    }

    const PacketOutputNode* rootNode() const
    {
        return &m_graph;
    }

    PacketOutputGraph& graph()
    {
        return m_graph;
    }

    const PacketOutputGraph& graph() const
    {
        return m_graph;
    }

    bool empty() const
    {
        return m_graph.empty();
    }

    std::size_t nodeCount() const
    {
        return m_graph.nodeCount();
    }

private:
    PacketOutputGraph m_graph;
    std::vector<std::unique_ptr<PacketOutputNode>> m_ownedNodes;
};

} // namespace media::ffmpeg
