#pragma once

#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"
#include "internal/graph/packet/PacketOutputNode.h"

#include <algorithm>
#include <cstddef>
#include <vector>

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg {

class PacketOutputGraph final : public PacketOutputNode {
public:
    void clear()
    {
        m_nodes.clear();
    }

    Status attachNode(PacketOutputNode* node)
    {
        if (!node) {
            return Status::failure(ErrorInfo::invalidArgument(
                "PacketOutputGraph attachNode failed: node is null"));
        }

        if (containsNode(node)) {
            return Status::success();
        }

        m_nodes.push_back(node);
        return Status::success();
    }

    Status detachNode(PacketOutputNode* node)
    {
        if (!node) {
            return Status::failure(ErrorInfo::invalidArgument(
                "PacketOutputGraph detachNode failed: node is null"));
        }

        const auto it = std::find(m_nodes.begin(), m_nodes.end(), node);
        if (it == m_nodes.end()) {
            return Status::failure(ErrorInfo::invalidArgument(
                "PacketOutputGraph detachNode failed: node is not attached"));
        }

        m_nodes.erase(it);
        return Status::success();
    }

    bool containsNode(PacketOutputNode* node) const
    {
        return std::find(m_nodes.begin(), m_nodes.end(), node) != m_nodes.end();
    }

    bool empty() const
    {
        return m_nodes.empty();
    }

    std::size_t nodeCount() const
    {
        return m_nodes.size();
    }

    Status pushPacket(AVPacket* packet) override
    {
        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "PacketOutputGraph pushPacket failed: packet is null"));
        }

        if (m_nodes.empty()) {
            return Status::failure(ErrorInfo::notInitialized(
                "PacketOutputGraph pushPacket failed: no output nodes"));
        }

        for (auto* node : m_nodes) {
            if (!node) {
                continue;
            }

            PacketPtr clone = makePacket();
            if (!clone) {
                return Status::failure(makeAllocationError(
                    "PacketOutputGraph failed to allocate packet"));
            }

            const int ret = av_packet_ref(clone.get(), packet);
            if (ret < 0) {
                return Status::failure(makeFFmpegError(
                    "PacketOutputGraph av_packet_ref failed", ret));
            }

            Status status = node->pushPacket(clone.get());
            if (!status) {
                return status;
            }
        }

        return Status::success();
    }

private:
    std::vector<PacketOutputNode*> m_nodes;
};

} // namespace media::ffmpeg
