#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/model/MediaPacketSourceTiming.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media::ffmpeg::graph {

class FFmpegPacketBuffer final : public MediaBuffer {
public:
    FFmpegPacketBuffer(::media::ffmpeg::PacketPtr packet,
                       std::optional<MediaPacketSourceTiming> sourceTiming);

    MediaBufferType type() const noexcept override;
    // Counts packet payload plus every FFmpeg side-data payload. Object allocator
    // overhead is owned by the runtime pool budget rather than this payload budget.
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;

    AVPacket* packet() noexcept;
    const AVPacket* packet() const noexcept;
    ::media::ffmpeg::PacketPtr takePacket() noexcept;
    const std::optional<MediaPacketSourceTiming>& sourceTiming() const noexcept;

private:
    ::media::ffmpeg::PacketPtr m_packet;
    std::optional<MediaPacketSourceTiming> m_sourceTiming;
};

} // namespace media::ffmpeg::graph
