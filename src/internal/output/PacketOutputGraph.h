#pragma once

#include "internal/output/PacketOutputNode.h"
#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"

#include <vector>
#include <algorithm>

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

    void addNode(PacketOutputNode* node)
    {
        if (!node) return;
        m_nodes.push_back(node);
    }

    size_t nodeCount() const
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

        for (auto node : m_nodes) {
            if (!node) continue;

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

            Status st = node->pushPacket(clone.get());
            if (!st) {
                return st;
            }
        }

        return Status::success();
    }

private:
    std::vector<PacketOutputNode*> m_nodes;
};

} // namespace media::ffmpeg
