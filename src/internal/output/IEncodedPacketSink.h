#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg {

class IEncodedPacketSink {
public:
    virtual ~IEncodedPacketSink() = default;

    IEncodedPacketSink(const IEncodedPacketSink&) = delete;
    IEncodedPacketSink& operator=(const IEncodedPacketSink&) = delete;

    virtual Status writePacket(AVPacket* packet) = 0;

protected:
    IEncodedPacketSink() = default;
};

} // namespace media::ffmpeg
