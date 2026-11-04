#pragma once
#include "internal/graph/planner/realtime/MediaTsSeriesStorageGeometry.h"
#include "media_transcode/Result.h"
#include <cstddef>

namespace media::ffmpeg::graph {
// Series and packetizer storage only. Framing/tables remain separate products.
struct MediaTsSeriesStoragePlan final {
    std::size_t maximumPackets;
    std::size_t maximumDatagrams;
    std::size_t maximumLiveSeries;
    std::size_t maximumPrefixViews;
    MediaTsPacketizerSeriesStorageGeometry packetizerGeometry;
    MediaTsProtocolSeriesStorageGeometry protocolGeometry;
    MediaSharedAllocationGeometry packetizerControlGeometry;
    MediaSharedAllocationGeometry continuityReservationGeometry;
    MediaSharedAllocationGeometry acceptedObservationGeometry;
    std::size_t continuityLedgerCount;
    std::size_t allocationBytes;
    friend bool operator==(const MediaTsSeriesStoragePlan&, const MediaTsSeriesStoragePlan&) = default;
};
class MediaTsSeriesStoragePlanner final {
public:
    static ::media::Result<MediaTsSeriesStoragePlan> plan(std::size_t maximumPackets,
        std::size_t maximumDatagrams, std::size_t maximumLiveSeries,
        std::size_t maximumPrefixViews);
    static ::media::Status validate(const MediaTsSeriesStoragePlan& product);
};
} // namespace media::ffmpeg::graph
