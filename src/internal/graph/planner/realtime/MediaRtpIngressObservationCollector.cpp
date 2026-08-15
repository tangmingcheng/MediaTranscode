#include "internal/graph/planner/realtime/MediaRtpIngressObservationCollector.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {

::media::Status MediaRtpIngressObservationCollector::observe(
    std::size_t datagramBytes,
    std::uint16_t sequenceNumber,
    std::int64_t observedAtNanoseconds)
{
    if (datagramBytes == 0 || observedAtNanoseconds <= 0 ||
        m_observedDatagrams == (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress observation requires bounded datagram and monotonic time facts"));
    }
    if (m_lastObservedAtNanoseconds &&
        observedAtNanoseconds < *m_lastObservedAtNanoseconds) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress observation time regressed"));
    }
    if (!m_firstObservedAtNanoseconds) {
        m_firstObservedAtNanoseconds = observedAtNanoseconds;
    }
    if (m_lastObservedAtNanoseconds) {
        m_maximumInterarrivalNanoseconds = (std::max)(
            m_maximumInterarrivalNanoseconds,
            observedAtNanoseconds - *m_lastObservedAtNanoseconds);
    }
    m_lastObservedAtNanoseconds = observedAtNanoseconds;
    m_maximumDatagramBytes = (std::max)(
        m_maximumDatagramBytes, datagramBytes);
    if (m_highestSequence) {
        const std::int16_t distance = static_cast<std::int16_t>(
            sequenceNumber - *m_highestSequence);
        if (distance > 0) {
            m_highestSequence = sequenceNumber;
        } else if (distance < 0) {
            m_maximumSequenceDisplacementPackets = (std::max)(
                m_maximumSequenceDisplacementPackets,
                static_cast<std::size_t>(-static_cast<int>(distance)));
        }
    } else {
        m_highestSequence = sequenceNumber;
    }
    ++m_observedDatagrams;
    return ::media::Status::success();
}

::media::Result<MediaRtpIngressObservation>
MediaRtpIngressObservationCollector::seal() const
{
    if (!m_firstObservedAtNanoseconds || !m_lastObservedAtNanoseconds) {
        return ::media::Result<MediaRtpIngressObservation>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP ingress observation has no source evidence"));
    }
    return MediaRtpIngressObservation::create({
        m_maximumDatagramBytes,
        m_maximumSequenceDisplacementPackets,
        m_maximumInterarrivalNanoseconds,
        m_observedDatagrams,
        *m_lastObservedAtNanoseconds - *m_firstObservedAtNanoseconds});
}

void MediaRtpIngressObservationCollector::reset() noexcept
{
    m_maximumDatagramBytes = 0;
    m_maximumSequenceDisplacementPackets = 0;
    m_maximumInterarrivalNanoseconds = 0;
    m_observedDatagrams = 0;
    m_highestSequence.reset();
    m_firstObservedAtNanoseconds.reset();
    m_lastObservedAtNanoseconds.reset();
}

} // namespace media::ffmpeg::graph
