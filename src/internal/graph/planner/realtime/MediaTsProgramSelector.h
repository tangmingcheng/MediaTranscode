#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramSelection.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"

namespace media::ffmpeg::graph {

class MediaTsProgramSelector final {
public:
    static ::media::Result<MediaTsVideoOnlyProgramSelection> selectVideoOnly(
        const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
        const MediaTsProgramInventorySnapshot& parserInventory,
        int selectedVideoStream,
        MediaRational videoTimeBase);
    static ::media::Result<MediaTsAudioVideoProgramSelection> selectAudioVideo(
        const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
        const MediaTsProgramInventorySnapshot& parserInventory,
        int selectedVideoStream,
        MediaRational videoTimeBase,
        int selectedAudioStream,
        MediaRational audioTimeBase);
};

} // namespace media::ffmpeg::graph
