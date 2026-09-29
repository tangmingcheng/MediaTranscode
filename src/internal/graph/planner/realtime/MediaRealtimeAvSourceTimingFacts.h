#pragma once

#include "internal/graph/protocol/mpegts/MediaTsPacketDurationEvidence.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSourceTimingFacts {
    std::optional<std::string> inputVideoIdentity;
    std::optional<std::string> inputAudioIdentity;
    std::optional<int> inputVideoClockRate;
    std::optional<int> inputAudioSampleRate;
    std::optional<std::uint32_t> inputAudioSamplesPerAccessUnit;
    std::optional<MediaTsPacketDurationEvidence> inputVideoPacketDuration;
    std::optional<MediaTsPacketDurationEvidence> inputAudioPacketDuration;
    friend bool operator==(const MediaRealtimeAvSourceTimingFacts&,
                           const MediaRealtimeAvSourceTimingFacts&) = default;
};

} // namespace media::ffmpeg::graph
