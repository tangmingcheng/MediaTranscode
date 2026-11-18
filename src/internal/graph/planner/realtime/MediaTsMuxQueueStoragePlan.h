#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {
// Final-topology queue backing only; does not claim series/cursor storage coverage.
struct MediaTsMuxQueueStoragePlan final {
    std::uint64_t maximumQueuedBytes;
    std::size_t slots;
    std::size_t allocationBytes;
    friend bool operator==(const MediaTsMuxQueueStoragePlan&, const MediaTsMuxQueueStoragePlan&) = default;
};
class MediaTsMuxQueueStoragePlanner final {
public:
    static ::media::Result<MediaTsMuxQueueStoragePlan> plan(std::uint64_t maximumQueuedBytes);
    static ::media::Status validate(const MediaTsMuxQueueStoragePlan& plan);
};
} // namespace media::ffmpeg::graph
