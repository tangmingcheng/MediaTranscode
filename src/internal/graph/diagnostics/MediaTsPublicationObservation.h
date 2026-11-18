#pragma once

#include "internal/graph/runtime/network/MediaDatagramSubmittedService.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// Value-only evidence from the exact accepted-work snapshot consumed by planning.
// These facts are never scheduling inputs after the publication is constructed.
struct MediaTsAcceptedWorkObservation final {
    std::uint64_t generation;
    MediaRunningTime observedNow;
    std::optional<std::uint64_t> lastCommittedSequence;
    std::optional<MediaDatagramSubmittedService> submittedService;
    MediaRunningTime projectedAvailableAt;
    friend bool operator==(const MediaTsAcceptedWorkObservation&,
                           const MediaTsAcceptedWorkObservation&) = default;
};

// Reservation identity is local to the bound packetizer, within this generation.
// Each published view owns a copy so a subsequent prefix cannot overwrite it.
struct MediaTsPublicationObservation final {
    MediaTsAcceptedWorkObservation acceptedWork;
    std::uint64_t cursorIdentity;
    std::uint64_t cursorRevision;
    std::uint64_t publicationIdentity;
    std::size_t protocolBegin;
    std::size_t protocolEnd;
    std::optional<MediaRunningTime> muxPublicationReadyAt;
    friend bool operator==(const MediaTsPublicationObservation&,
                           const MediaTsPublicationObservation&) = default;
};

} // namespace media::ffmpeg::graph
