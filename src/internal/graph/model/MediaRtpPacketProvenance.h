#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

// Raw ingress identity, before calibration into a clock-group generation.
// Sequence shares the producing RawRtpInputNode's clock/event ordering.
struct MediaRtpPacketProvenance final {
    std::uint32_t ssrc;
    std::uint64_t ingressGeneration;
    std::uint64_t ingressSequence;
};

} // namespace media::ffmpeg::graph
