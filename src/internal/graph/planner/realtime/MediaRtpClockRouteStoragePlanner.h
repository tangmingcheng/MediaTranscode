#pragma once
#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "media_transcode/Result.h"
#include <span>

namespace media::ffmpeg::graph {
struct MediaRtpClockRouteStoragePlan final {
    std::size_t maximumCnameBytes;
    std::size_t evidenceQueueBytes;
    std::size_t evidenceInFlightBytes;
    std::size_t snapshotQueueBytes;
    std::size_t snapshotInFlightBytes;
    std::size_t binderReplacementBytes;
    std::size_t additionalCnameBytes;
};
// Additional runtime route ownership. Excludes source-owned tracker/mapper/
// validator, source output queues and already counted locked binder snapshots.
class MediaRtpClockRouteStoragePlanner final {
public:
    static ::media::Result<MediaRtpClockRouteStoragePlan> plan(
        MediaTranscodeStreamSet members, const MediaEdgePolicy& metadata,
        std::span<const std::size_t> maximumDatagramBytes);
};
}
