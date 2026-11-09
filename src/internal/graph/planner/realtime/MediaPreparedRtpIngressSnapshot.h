#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressObservation.h"
#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <chrono>
#include <stop_token>

namespace media::ffmpeg::graph {

class MediaRawRtpPreparedInputBuffer;

struct MediaRtpIngressSnapshotWait final {
    std::chrono::steady_clock::time_point deadline;
    std::stop_token cancellation;
};

// Captured under one input-buffer lock. Identity belongs to one stream buffer,
// not the AV-shared byte budget, and never extends the source lifetime.
struct MediaPreparedRtpIngressSnapshot final {
    MediaRtpIngressObservation observation;
    std::uint64_t ingressFrontier;
    std::weak_ptr<const MediaRawRtpPreparedInputBuffer> source;
    std::size_t socketReceiveCapacityBytes;
    std::size_t preparedWireBytes;
    std::size_t maximumDatagramBytes;
    MediaRtpReceiveGeometry receiveGeometry;
};

} // namespace media::ffmpeg::graph
