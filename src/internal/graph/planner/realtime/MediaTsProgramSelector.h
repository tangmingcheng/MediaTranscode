#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "internal/graph/protocol/mpegts/MediaTsPreflightDurationProbe.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaTsSelectedProgramPlan final {
    int programNumber = 0;
    int programMapPid = 0;
    int videoPid = 0;
    int audioPid = 0;
    int pcrPid = 0;
    std::optional<MediaTsPacketDurationEvidence> videoPacketDuration;
    std::optional<MediaTsPacketDurationEvidence> audioPacketDuration;
};

class MediaTsProgramSelector final {
public:
    static ::media::Result<MediaTsSelectedProgramPlan> select(
        const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
        const MediaTsProgramInventorySnapshot& parserInventory,
        int selectedVideoStream,
        int selectedAudioStream);
};

} // namespace media::ffmpeg::graph
