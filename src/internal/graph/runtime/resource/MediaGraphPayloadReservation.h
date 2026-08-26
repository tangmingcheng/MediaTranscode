#pragma once

#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

class MediaBuffer;

class MediaGraphPayloadReservation final {
public:
    MediaGraphPayloadReservation() = default;
    MediaGraphPayloadReservation(
        MediaGraphPayloadAllocationAccounting accounting,
        std::uint64_t maximumReservationBytes,
        MediaGraphPayloadCreditLease lease);

    ::media::Status shrinkToActual(std::uint64_t bytes) noexcept;
    ::media::Status attachTo(MediaBuffer& buffer) noexcept;
    explicit operator bool() const noexcept;

private:
    MediaGraphPayloadAllocationAccounting m_accounting =
        MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject;
    std::uint64_t m_maximumReservationBytes = 0;
    std::shared_ptr<MediaGraphPayloadCreditLease> m_lease;
};

} // namespace media::ffmpeg::graph
