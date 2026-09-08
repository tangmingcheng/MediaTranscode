#include "internal/graph/planner/realtime/MediaRtpIngressObservation.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRtpIngressObservation::MediaRtpIngressObservation(
    MediaRtpIngressObservationFacts facts) noexcept
    : m_facts(facts)
{
}

::media::Result<MediaRtpIngressObservation>
MediaRtpIngressObservation::create(MediaRtpIngressObservationFacts facts)
{
    MediaRtpIngressObservation product(facts);
    if (auto status = product.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressObservation>::failure(
            status.error());
    }
    return ::media::Result<MediaRtpIngressObservation>::success(
        std::move(product));
}

std::size_t MediaRtpIngressObservation::maximumDatagramBytes() const noexcept
{
    return m_facts.maximumDatagramBytes;
}

std::size_t
MediaRtpIngressObservation::maximumSequenceDisplacementPackets() const noexcept
{
    return m_facts.maximumSequenceDisplacementPackets;
}

std::int64_t
MediaRtpIngressObservation::maximumInterarrivalNanoseconds() const noexcept
{
    return m_facts.maximumInterarrivalNanoseconds;
}

std::size_t MediaRtpIngressObservation::observedDatagrams() const noexcept
{
    return m_facts.observedDatagrams;
}

std::int64_t
MediaRtpIngressObservation::observationSpanNanoseconds() const noexcept
{
    return m_facts.observationSpanNanoseconds;
}

::media::Status MediaRtpIngressObservation::validateProduct() const
{
    if (m_facts.maximumDatagramBytes == 0 ||
        m_facts.maximumSequenceDisplacementPackets >=
            m_facts.observedDatagrams ||
        m_facts.maximumInterarrivalNanoseconds <= 0 ||
        m_facts.observedDatagrams == 0 ||
        m_facts.observationSpanNanoseconds <
            m_facts.maximumInterarrivalNanoseconds) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress observation requires complete positive source evidence"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
