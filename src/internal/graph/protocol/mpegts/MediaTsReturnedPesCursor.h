#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsPesProvenanceTimeline.h"

#include <unordered_map>
#include <functional>

namespace media::ffmpeg::graph {

class MediaTsReturnedPesCursor final {
public:
    using AnchorResolver = std::function<::media::Result<MediaTsPesProvenanceAnchor>(
        std::uint64_t, std::uint16_t)>;
    using StateResolver = std::function<::media::Result<MediaTsPesProvenanceAnchor>(
        const MediaTsPesProvenanceAnchor&)>;
    static ::media::Result<MediaTsReturnedPesCursor> create(
        const MediaTsRuntimeBinding& binding);

    ::media::Result<MediaTsPacketProvenance> resolve(
        int streamIndex,
        std::int64_t packetPosition,
        const AnchorResolver& resolveAnchor,
        const StateResolver& resolveState);

private:
    struct StreamCursor final {
        std::uint16_t pid = 0;
        std::optional<MediaTsPesProvenanceAnchor> active;
    };

    std::unordered_map<int, StreamCursor> m_streams;
};

} // namespace media::ffmpeg::graph
