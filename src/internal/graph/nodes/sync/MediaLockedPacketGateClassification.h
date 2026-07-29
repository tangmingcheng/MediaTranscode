#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionSnapshot;
struct MediaAvReacquisitionSnapshot;

enum class MediaLockedPacketGateDisposition : std::uint8_t {
    Pass = 0,
    WithholdForReacquisition = 1,
    DropOldGeneration = 2,
    PassToReacquisition = 3,
    PassToInitialAcquisition = 4
};

::media::Result<MediaLockedPacketGateDisposition>
classifyLockedPacketGateGeneration(
    const MediaAvReacquisitionSnapshot& reacquisition,
    const MediaAvEpochTransitionSnapshot& epoch,
    std::uint64_t generation,
    std::uint64_t plannedInitialGeneration);

} // namespace media::ffmpeg::graph
