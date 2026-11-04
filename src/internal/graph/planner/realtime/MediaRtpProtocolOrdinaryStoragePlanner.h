#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"

namespace media::ffmpeg::graph {

// Remaining ordinary-allocator queue/assembly owners. Public AVPacket payload
// and headers belong to the separate live payload leases; private codec/FFmpeg
// internals and process RSS are not this engine storage domain.
class MediaRtpProtocolOrdinaryStoragePlanner final {
public:
    static ::media::Result<std::size_t> plan(const MediaPreparedRtpSourceSessionStreamPlan& stream);
};

} // namespace media::ffmpeg::graph
