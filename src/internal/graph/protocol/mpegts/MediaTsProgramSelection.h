#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketDurationEvidence.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaTsSelectedStreamPlan final {
    int streamIndex = -1;
    int elementaryPid = 0;
    MediaRational timeBase;
    friend bool operator==(const MediaTsSelectedStreamPlan& left,
                           const MediaTsSelectedStreamPlan& right) noexcept
    {
        return left.streamIndex == right.streamIndex &&
            left.elementaryPid == right.elementaryPid &&
            left.timeBase.num == right.timeBase.num &&
            left.timeBase.den == right.timeBase.den;
    }
};

struct MediaTsVideoOnlyProgramSelection final {
    int programNumber = 0;
    int programMapPid = 0;
    int pcrPid = 0;
    MediaTsSelectedStreamPlan video;
    bool operator==(const MediaTsVideoOnlyProgramSelection&) const = default;
};

struct MediaTsAudioVideoProgramSelection final {
    int programNumber = 0;
    int programMapPid = 0;
    int pcrPid = 0;
    MediaTsSelectedStreamPlan video;
    MediaTsSelectedStreamPlan audio;
    bool operator==(const MediaTsAudioVideoProgramSelection&) const = default;
};

using MediaTsProgramSelection = std::variant<
    MediaTsVideoOnlyProgramSelection,
    MediaTsAudioVideoProgramSelection>;

struct MediaTsVideoOnlySelectedProgramPlan final {
    MediaTsVideoOnlyProgramSelection selection;
    MediaTsPacketDurationEvidence videoPacketDuration;
    bool operator==(const MediaTsVideoOnlySelectedProgramPlan&) const = default;
};

struct MediaTsAudioVideoSelectedProgramPlan final {
    MediaTsAudioVideoProgramSelection selection;
    MediaTsPacketDurationEvidence videoPacketDuration;
    MediaTsPacketDurationEvidence audioPacketDuration;
    bool operator==(const MediaTsAudioVideoSelectedProgramPlan&) const = default;
};

using MediaTsSelectedProgramPlan = std::variant<
    MediaTsVideoOnlySelectedProgramPlan,
    MediaTsAudioVideoSelectedProgramPlan>;

} // namespace media::ffmpeg::graph
