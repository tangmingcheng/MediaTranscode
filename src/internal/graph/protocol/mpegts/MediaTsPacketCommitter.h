#pragma once

#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"

namespace media::ffmpeg::graph {

class MediaTsPacketCommitter {
public:
    virtual ~MediaTsPacketCommitter() = default;
    virtual ::media::Status commit(
        MediaTsPacketCursor& cursor,
        MediaTsPacketCommitToken token) = 0;
};

class MediaTsPacketCursorCommitter final : public MediaTsPacketCommitter {
public:
    ::media::Status commit(
        MediaTsPacketCursor& cursor,
        MediaTsPacketCommitToken token) override;
};

} // namespace media::ffmpeg::graph
