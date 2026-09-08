#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpIngressAdapterAvailability final {
    MediaRtpIngressAdapterKind adapterKind;
    bool available;
    std::string unavailableReason;
};

class MediaRtpIngressCapabilityScanner final {
public:
    static ::media::Result<MediaRtpIngressAdapterKind> select(
        std::vector<MediaRtpIngressAdapterAvailability> candidates);

private:
    MediaRtpIngressCapabilityScanner() = delete;
};

} // namespace media::ffmpeg::graph
