#pragma once
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

namespace media::ffmpeg::graph {

// Engine allocations and explicit public-header credit only; referenced source
// lineage and decoder/FFmpeg-private storage belong to their existing owners.
struct MediaRtpSourceIdentityProbeStorage final {
    MediaCodecLineagePreparedStorage registry;
    std::size_t bootstrapBytes;
    std::size_t probeBytes;
    std::size_t publicHeaderBytes;
    std::size_t capacityBytes;
    std::size_t budgetBytes;
    std::size_t totalBytes;
};

class MediaRtpSourceIdentityProbeStoragePlanner final {
public:
    static ::media::Result<MediaRtpSourceIdentityProbeStorage> plan(
        std::size_t maximumSubmissions, std::size_t additionalSoftwareFrameBytes);
};

} // namespace media::ffmpeg::graph
