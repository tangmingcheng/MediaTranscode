#pragma once
#include "internal/graph/planner/realtime/MediaTsMuxQueueStoragePlan.h"
#include "internal/graph/planner/realtime/MediaTsSeriesStoragePlan.h"
#include "internal/graph/planner/realtime/MediaDatagramProtocolTopology.h"
#include "media_transcode/Result.h"
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {
struct MediaMpegTsProducerPlan final {
    std::uint64_t maximumDatagramBytes;
    std::uint64_t maximumRtcpCompoundBytes;
    std::optional<MediaTsMuxQueueStoragePlan> muxQueueStorage;
    std::optional<MediaTsSeriesStoragePlan> seriesStorage;
    friend bool operator==(const MediaMpegTsProducerPlan&, const MediaMpegTsProducerPlan&) = default;
};
::media::Result<std::optional<MediaMpegTsProducerPlan>> planMpegTsProducer(
    MediaDatagramProtocolLayout layout, std::size_t maximumUdpPayloadBytes,
    std::string_view cname);
::media::Status validateMpegTsProducerPlan(MediaDatagramProtocolLayout layout,
    const std::optional<MediaMpegTsProducerPlan>& producer);
} // namespace media::ffmpeg::graph
