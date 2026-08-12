#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramSelection.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaTsProgramContractValidator final {
public:
    static bool isSelectedStreamTimeBase(MediaRational timeBase) noexcept;
    static ::media::Status validateSnapshots(
        const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
        const MediaTsProgramInventorySnapshot& parserInventory);
    static ::media::Status validateSelection(
        const MediaTsProgramSelection& selection);
    static ::media::Status validateSelectedProgram(
        const MediaTsSelectedProgramPlan& selectedProgram);

private:
    MediaTsProgramContractValidator() = delete;
};

} // namespace media::ffmpeg::graph
