#pragma once

#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaWireTrafficEnvelope final {
    std::uint64_t sustainedWireBytesPerSecond = 0;
    std::uint64_t peakWireBytesPerSecond = 0;
    std::uint64_t peakDatagramsPerSecond = 0;
    std::uint64_t burstWireBytes = 0;
    std::uint64_t burstDatagrams = 0;
    std::uint64_t maximumAtomicWireBytes = 0;
    std::uint64_t maximumAtomicDatagrams = 0;
    std::uint64_t maximumUdpPayloadBytes = 0;
    std::uint64_t maximumWireDatagramBytes = 0;
    std::string authority;

    friend bool operator==(const MediaWireTrafficEnvelope&,
                           const MediaWireTrafficEnvelope&) = default;
};

} // namespace media::ffmpeg::graph
