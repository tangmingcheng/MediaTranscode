#pragma once

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaPreparedAudioEncoderEmissionEnvelope final {
    std::uint64_t sustainedPayloadBytesPerSecond = 0;
    std::uint64_t peakPayloadBytesPerSecond = 0;
    std::uint64_t maximumAccessUnitPayloadBytes = 0;
    std::uint64_t maximumBurstPayloadBytes = 0;
    std::uint64_t accessUnitsPerSecondNumerator = 0;
    std::uint64_t accessUnitsPerSecondDenominator = 0;
    std::uint64_t maximumPacketizationUnitsPerAccessUnit = 0;
    int frameSizeSamples = 0;
    std::string authority;
    std::string backend;

    friend bool operator==(
        const MediaPreparedAudioEncoderEmissionEnvelope&,
        const MediaPreparedAudioEncoderEmissionEnvelope&) = default;
};

} // namespace media::ffmpeg::graph
