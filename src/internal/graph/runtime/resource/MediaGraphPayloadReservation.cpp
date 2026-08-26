#include "internal/graph/runtime/resource/MediaGraphPayloadReservation.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaGraphPayloadReservation::MediaGraphPayloadReservation(
    MediaGraphPayloadAllocationAccounting accounting,
    std::uint64_t maximumReservationBytes,
    MediaGraphPayloadCreditLease lease)
    : m_accounting(accounting)
    , m_maximumReservationBytes(maximumReservationBytes)
    , m_lease(std::make_shared<MediaGraphPayloadCreditLease>(
          std::move(lease)))
{
}

MediaGraphPayloadReservation
MediaGraphPayloadReservation::nonRealtimeNotApplicable() noexcept
{
    MediaGraphPayloadReservation reservation;
    reservation.m_nonRealtimeNotApplicable = true;
    return reservation;
}

::media::Status MediaGraphPayloadReservation::shrinkToActual(
    std::uint64_t bytes) noexcept
{
    if (m_nonRealtimeNotApplicable) return ::media::Status::success();
    if (!m_lease || !*m_lease || bytes == 0 ||
        bytes > m_maximumReservationBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "payload producer exceeded its prepared single-unit bound"));
    }
    if (m_accounting ==
        MediaGraphPayloadAllocationAccounting::
            ObservedOnlyExternalBytesAndEngineManagedObject) {
        return ::media::Status::success();
    }
    return m_lease->shrinkTo(bytes);
}

::media::Status MediaGraphPayloadReservation::attachTo(
    MediaBuffer& buffer) noexcept
{
    if (m_nonRealtimeNotApplicable) return ::media::Status::success();
    if (!m_lease || !*m_lease) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "payload reservation has no active credit lease"));
    }
    return buffer.attachPayloadCredit(std::move(m_lease));
}

::media::Status MediaGraphPayloadReservation::shareWithAliasingBuffer(
    MediaBuffer& buffer) const noexcept
{
    if (m_nonRealtimeNotApplicable) return ::media::Status::success();
    if (!m_lease || !*m_lease) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "payload reservation has no active alias credit lease"));
    }
    return buffer.attachPayloadCredit(m_lease);
}

MediaGraphPayloadReservation::operator bool() const noexcept
{
    return m_nonRealtimeNotApplicable || (m_lease && *m_lease);
}

} // namespace media::ffmpeg::graph
