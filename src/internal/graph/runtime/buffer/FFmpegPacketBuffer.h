#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg::graph {

class FFmpegPacketBuffer final : public MediaBuffer {
public:
    explicit FFmpegPacketBuffer(::media::ffmpeg::PacketPtr packet);

    MediaBufferType type() const noexcept override;

    AVPacket* packet() noexcept;
    const AVPacket* packet() const noexcept;
    ::media::ffmpeg::PacketPtr takePacket() noexcept;

private:
    ::media::ffmpeg::PacketPtr m_packet;
};

} // namespace media::ffmpeg::graph
