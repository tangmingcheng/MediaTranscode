#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRtpIngressAdapterFactory final {
public:
    static ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>> create(
        MediaRtpUdpTransport transport,
        const MediaRtpIngressPlan& plan);

private:
    MediaRtpIngressAdapterFactory() = delete;
};

} // namespace media::ffmpeg::graph
