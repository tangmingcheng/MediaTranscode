#pragma once

#include "internal/graph/model/MediaDemuxPacketProvenance.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <deque>

namespace media::ffmpeg::graph {

struct MediaDemuxPreparedPacket final {
    ::media::ffmpeg::PacketPtr packet;
    MediaDemuxPacketProvenance provenance;
};

struct MediaDemuxInputSession final {
    ::media::ffmpeg::InputFormatContextPtr context;
    std::deque<MediaDemuxPreparedPacket> replay;
    std::uint64_t nextLiveOrdinal = 0;
};

class MediaDemuxInputBuffer {
public:
    virtual ~MediaDemuxInputBuffer() = default;
    virtual ::media::Result<MediaDemuxInputSession> takeDemuxSession() = 0;
};

} // namespace media::ffmpeg::graph
