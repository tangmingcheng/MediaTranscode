#pragma once

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

// Published with the final source cut. Payload guarantees include ordinary
// compressed assembly; metadata and graph payload retain their own accounts.
struct MediaPreparedRtpCreditPlan final {
    std::uint64_t heldPayloadBytes;
    std::uint64_t fixedAssemblyBytes;
    std::uint64_t maximumTransactionBytes;
    std::uint64_t guaranteedPayloadBytes;
    std::size_t maximumTransactionUnits;
    std::size_t maximumPacketObjects;
    std::size_t maximumControlObjects;
    std::size_t maximumPacketBytes;
    std::size_t maximumControlBytes;

    bool operator==(const MediaPreparedRtpCreditPlan&) const noexcept = default;
};

} // namespace media::ffmpeg::graph
