#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaAvGenerationParticipant : std::uint8_t {
    CanonicalLineage = 0,
    AudioCorrection = 1,
    Scheduler = 2,
    RtpVideoOutput = 3,
    RtpAudioOutput = 4,
    ProjectMpegTsOutput = 5
};

struct MediaAvGenerationParticipantPlan final {
    MediaAvGenerationParticipant participant;
    std::vector<std::string> requiredChildren;
};

struct MediaAvGenerationTransitionPlan final {
    std::vector<MediaAvGenerationParticipantPlan> participants;
    MediaRunningTime acknowledgementTimeout;
    MediaRunningTime terminalDrainWindow;
};

} // namespace media::ffmpeg::graph
