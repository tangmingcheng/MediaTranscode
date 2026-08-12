#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketDurationEvidence.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaTsSelectedStreamPlan final {
    MediaTsSelectedStreamPlan() = delete;
    MediaTsSelectedStreamPlan(
        int selectedStreamIndex,
        int selectedElementaryPid,
        MediaRational selectedTimeBase) noexcept
        : streamIndex(selectedStreamIndex),
          elementaryPid(selectedElementaryPid),
          timeBase(selectedTimeBase)
    {
    }

    int streamIndex;
    int elementaryPid;
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
    MediaTsVideoOnlyProgramSelection() = delete;
    MediaTsVideoOnlyProgramSelection(
        int selectedProgramNumber,
        int selectedProgramMapPid,
        int selectedPcrPid,
        MediaTsSelectedStreamPlan selectedVideo) noexcept
        : programNumber(selectedProgramNumber),
          programMapPid(selectedProgramMapPid),
          pcrPid(selectedPcrPid),
          video(selectedVideo)
    {
    }

    int programNumber;
    int programMapPid;
    int pcrPid;
    MediaTsSelectedStreamPlan video;
    bool operator==(const MediaTsVideoOnlyProgramSelection&) const = default;
};

struct MediaTsAudioVideoProgramSelection final {
    MediaTsAudioVideoProgramSelection() = delete;
    MediaTsAudioVideoProgramSelection(
        int selectedProgramNumber,
        int selectedProgramMapPid,
        int selectedPcrPid,
        MediaTsSelectedStreamPlan selectedVideo,
        MediaTsSelectedStreamPlan selectedAudio) noexcept
        : programNumber(selectedProgramNumber),
          programMapPid(selectedProgramMapPid),
          pcrPid(selectedPcrPid),
          video(selectedVideo),
          audio(selectedAudio)
    {
    }

    int programNumber;
    int programMapPid;
    int pcrPid;
    MediaTsSelectedStreamPlan video;
    MediaTsSelectedStreamPlan audio;
    bool operator==(const MediaTsAudioVideoProgramSelection&) const = default;
};

using MediaTsProgramSelection = std::variant<
    MediaTsVideoOnlyProgramSelection,
    MediaTsAudioVideoProgramSelection>;

struct MediaTsVideoOnlySelectedProgramPlan final {
    MediaTsVideoOnlySelectedProgramPlan() = delete;
    MediaTsVideoOnlySelectedProgramPlan(
        MediaTsVideoOnlyProgramSelection selected,
        MediaTsPacketDurationEvidence selectedVideoPacketDuration) noexcept
        : selection(selected),
          videoPacketDuration(selectedVideoPacketDuration)
    {
    }

    MediaTsVideoOnlyProgramSelection selection;
    MediaTsPacketDurationEvidence videoPacketDuration;
    bool operator==(const MediaTsVideoOnlySelectedProgramPlan&) const = default;
};

struct MediaTsAudioVideoSelectedProgramPlan final {
    MediaTsAudioVideoSelectedProgramPlan() = delete;
    MediaTsAudioVideoSelectedProgramPlan(
        MediaTsAudioVideoProgramSelection selected,
        MediaTsPacketDurationEvidence selectedVideoPacketDuration,
        MediaTsPacketDurationEvidence selectedAudioPacketDuration) noexcept
        : selection(selected),
          videoPacketDuration(selectedVideoPacketDuration),
          audioPacketDuration(selectedAudioPacketDuration)
    {
    }

    MediaTsAudioVideoProgramSelection selection;
    MediaTsPacketDurationEvidence videoPacketDuration;
    MediaTsPacketDurationEvidence audioPacketDuration;
    bool operator==(const MediaTsAudioVideoSelectedProgramPlan&) const = default;
};

using MediaTsSelectedProgramPlan = std::variant<
    MediaTsVideoOnlySelectedProgramPlan,
    MediaTsAudioVideoSelectedProgramPlan>;

} // namespace media::ffmpeg::graph
