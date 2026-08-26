#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaPreparedEncoderEmissionEnvelope final {
    std::uint64_t sustainedPayloadBytesPerSecond = 0;
    std::uint64_t peakPayloadBytesPerSecond = 0;
    std::uint64_t maximumAccessUnitPayloadBytes = 0;
    std::uint64_t maximumBurstPayloadBytes = 0;
    std::uint64_t accessUnitsPerSecondNumerator = 0;
    std::uint64_t accessUnitsPerSecondDenominator = 0;
    std::uint64_t maximumEncoderRetainedFrames = 0;
    std::optional<MediaEncodedPacketLayout> encodedPacketLayout;
    std::string authority;
    std::string backend;

    friend bool operator==(
        const MediaPreparedEncoderEmissionEnvelope&,
        const MediaPreparedEncoderEmissionEnvelope&) = default;
};

} // namespace media::ffmpeg::graph
