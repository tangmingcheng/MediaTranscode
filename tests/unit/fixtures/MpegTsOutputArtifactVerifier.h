#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "media_transcode/Result.h"

#include <filesystem>

namespace media_transcode::test {

class MpegTsOutputArtifactVerifier final {
public:
    static ::media::Status verify(
        const std::filesystem::path& path,
        const ::media::ffmpeg::graph::MediaTsMuxPlan& muxPlan,
        const ::media::ffmpeg::graph::MediaAvSyncPlan& avSyncPlan);

private:
    MpegTsOutputArtifactVerifier() = delete;
};

} // namespace media_transcode::test
