#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg {

class PacketOutputNode {
public:
    virtual ~PacketOutputNode() = default;

    PacketOutputNode(const PacketOutputNode&) = delete;
    PacketOutputNode& operator=(const PacketOutputNode&) = delete;

    virtual Status pushPacket(AVPacket* packet) = 0;

protected:
    PacketOutputNode() = default;
};

} // namespace media::ffmpeg
