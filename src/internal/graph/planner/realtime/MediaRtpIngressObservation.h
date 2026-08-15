#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaRtpIngressObservationFacts final {
    std::size_t maximumDatagramBytes;
    std::size_t maximumDatagramsPerReadiness;
    std::size_t maximumSequenceDisplacementPackets;
    std::int64_t maximumArrivalVariationNanoseconds;
    std::size_t observedDatagrams;
    std::int64_t observationSpanNanoseconds;
};

class MediaRtpIngressObservation final {
public:
    MediaRtpIngressObservation() = delete;

    static ::media::Result<MediaRtpIngressObservation> create(
        MediaRtpIngressObservationFacts facts);

    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t maximumDatagramsPerReadiness() const noexcept;
    std::size_t maximumSequenceDisplacementPackets() const noexcept;
    std::int64_t maximumArrivalVariationNanoseconds() const noexcept;
    std::size_t observedDatagrams() const noexcept;
    std::int64_t observationSpanNanoseconds() const noexcept;
    ::media::Status validateProduct() const;

private:
    explicit MediaRtpIngressObservation(
        MediaRtpIngressObservationFacts facts) noexcept;

    MediaRtpIngressObservationFacts m_facts;
};

} // namespace media::ffmpeg::graph
