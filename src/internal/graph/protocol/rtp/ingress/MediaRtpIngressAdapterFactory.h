#pragma once

#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h"

#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRtpIngressAdapterFactory final {
public:
    static ::media::Result<std::size_t> preparationStorageBytes(
        const MediaRtpReceiveGeometry& geometry);
    static ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>> create(
        std::shared_ptr<MediaRtpUdpTransport> transport,
        const MediaRtpReceiveGeometry& plan,
        MediaLineageAllocationScope allocation);

    // Upper bound before sockets are opened; every final receive geometry
    // is bounded by this explicit wire capacity and datagram extent.
    static ::media::Result<std::size_t> maximumPreparationStorageBytes(
        std::size_t wireCapacity, std::size_t maximumDatagramBytes);

private:
    MediaRtpIngressAdapterFactory() = delete;
};

} // namespace media::ffmpeg::graph
