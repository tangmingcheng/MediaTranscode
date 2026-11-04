#pragma once
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include <optional>

namespace media::ffmpeg::graph {
struct MediaPreparedRealtimeEmissionSet;
struct MediaTsMediaSeriesBound final {
    std::size_t maximumFramedPayloadBytes;
    std::size_t maximumPackets;
    std::size_t maximumDatagrams;
    friend bool operator==(const MediaTsMediaSeriesBound&, const MediaTsMediaSeriesBound&) = default;
};
struct MediaTsProtocolSeriesBounds final {
    MediaTsMediaSeriesBound video;
    std::optional<MediaTsMediaSeriesBound> audio;
    std::size_t maximumPatPackets;
    std::size_t maximumPmtPackets;
    std::size_t maximumPcrPackets;
    friend bool operator==(const MediaTsProtocolSeriesBounds&, const MediaTsProtocolSeriesBounds&) = default;
};
class MediaTsProtocolSeriesBoundsPlanner final {
public:
    static ::media::Status validate(const MediaTsMuxPlan& mux,
        const MediaTsProtocolSeriesBounds& bounds);
    static ::media::Result<MediaTsProtocolSeriesBounds> plan(
        const MediaTsMuxPlan& mux,
        const MediaPreparedRealtimeEmissionSet& emission);
};
} // namespace media::ffmpeg::graph
