#pragma once

#include "internal/graph/model/MediaRawRtpPreparationStoragePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <cstdint>
#include <memory>
#include <memory_resource>

namespace media::ffmpeg::graph {

struct MediaPreparedRawRtpDatagram final {
    MediaRtpUdpDatagram datagram;
    std::int64_t observedAtNs;
    std::uint64_t ordinal;
};

using MediaPreparedRtpDatagramOwner = std::shared_ptr<const MediaPreparedRawRtpDatagram>;

class MediaPreparedRtpDatagramStorage final {
public:
    static ::media::Result<MediaPreparedRtpDatagramAllocationFacts> allocationFacts();
    // Throws bad_alloc on allocation or allocation-shape mismatch. The caller
    // converts this at its existing Result boundary. Preparation PMR allocations
    // pin their resource through control-block deallocation, including weak owners.
    static MediaPreparedRtpDatagramOwner create(std::pmr::memory_resource& resource,
        const MediaPreparedRtpDatagramAllocationFacts& allocation,
        MediaRtpUdpDatagram datagram, std::int64_t observedAtNs, std::uint64_t ordinal);
};

} // namespace media::ffmpeg::graph
