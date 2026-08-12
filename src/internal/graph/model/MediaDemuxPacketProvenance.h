#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaDemuxPacketOrigin : std::uint8_t {
    LiveDemuxRead = 0,
    PostFindStreamInfoPreparedRead = 1
};

struct MediaDemuxPacketProvenance final {
    MediaDemuxPacketOrigin origin;
    std::uint64_t ordinal;
    friend bool operator==(const MediaDemuxPacketProvenance&,
                           const MediaDemuxPacketProvenance&) = default;
};

} // namespace media::ffmpeg::graph
