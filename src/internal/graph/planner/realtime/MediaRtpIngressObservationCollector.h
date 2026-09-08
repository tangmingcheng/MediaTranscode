#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressObservation.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpIngressObservationCollector final {
public:
    ::media::Status observeDatagram(
        std::size_t datagramBytes,
        std::optional<std::uint16_t> rtpSequenceNumber,
        std::int64_t observedAtNanoseconds);
    ::media::Result<MediaRtpIngressObservation> seal();
    void reset() noexcept;

private:
    std::size_t m_maximumDatagramBytes = 0;
    std::size_t m_maximumSequenceDisplacementPackets = 0;
    std::int64_t m_maximumInterarrivalNanoseconds = 0;
    std::size_t m_observedDatagrams = 0;
    std::optional<std::uint16_t> m_highestSequence;
    std::optional<std::int64_t> m_firstObservedAtNanoseconds;
    std::optional<std::int64_t> m_lastObservedAtNanoseconds;
    bool m_sealed = false;
};

} // namespace media::ffmpeg::graph
