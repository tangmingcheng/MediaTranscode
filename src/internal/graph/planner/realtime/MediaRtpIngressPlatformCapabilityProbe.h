#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityScanner.h"

#include <vector>

namespace media::ffmpeg::graph {

class MediaRtpIngressPlatformCapabilityProbe final {
public:
    static ::media::Result<std::vector<MediaRtpIngressAdapterAvailability>>
    scan();

private:
    MediaRtpIngressPlatformCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
