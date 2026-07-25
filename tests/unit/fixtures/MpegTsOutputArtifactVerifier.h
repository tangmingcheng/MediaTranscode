#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "media_transcode/Result.h"

#include <filesystem>

namespace media_transcode::test {

class ScheduledMpegTsDecodeSampleFixture;

class MpegTsOutputArtifactVerifier final {
public:
    static ::media::Status verify(
        const std::filesystem::path& path,
        const ::media::ffmpeg::graph::MediaTsMuxPlan& plan,
        const ::media::ffmpeg::graph::MediaPlaybackEpoch& epoch,
        const ScheduledMpegTsDecodeSampleFixture& sample);

private:
    MpegTsOutputArtifactVerifier() = delete;
};

} // namespace media_transcode::test
