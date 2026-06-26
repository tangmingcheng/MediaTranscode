#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg {

class EncodedPacketSink {
public:
    virtual ~EncodedPacketSink() = default;

    EncodedPacketSink(const EncodedPacketSink&) = delete;
    EncodedPacketSink& operator=(const EncodedPacketSink&) = delete;

    virtual Status writePacket(AVPacket* packet) = 0;

protected:
    EncodedPacketSink() = default;
};

} // namespace media::ffmpeg
