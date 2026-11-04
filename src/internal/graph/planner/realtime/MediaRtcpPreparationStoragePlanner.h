#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
#include <span>

namespace media::ffmpeg::graph {

enum class MediaRtcpPreparationUse { ParseOnly, SourceClock };
struct MediaRtcpPreparationStoragePlan final {
    std::size_t parserTransientBytes;
    std::size_t maximumCnameBytes;
    std::size_t maximumCnameOwners;
    std::size_t cnamePeakBytes;
};
class MediaRtcpPreparationStoragePlanner final {
public:
    static ::media::Result<MediaRtcpPreparationStoragePlan> plan(
        std::span<const std::size_t> maximumDatagramBytes,
        std::size_t maximumQueuedEvidenceOwners, MediaRtcpPreparationUse use);
};

} // namespace media::ffmpeg::graph
