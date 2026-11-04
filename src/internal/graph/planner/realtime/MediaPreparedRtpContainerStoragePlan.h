#pragma once
#include <cstddef>
#include <optional>

namespace media::ffmpeg::graph {
struct MediaPreparedRtpContainerBounds final {
    std::size_t prefixObjects;
    std::size_t acquiringObjects;
    std::size_t outputObjects;
    bool operator==(const MediaPreparedRtpContainerBounds&) const = default;
};
struct MediaPreparedRtpSourceContainerBounds final {
    MediaPreparedRtpContainerBounds video;
    std::optional<MediaPreparedRtpContainerBounds> audio;
};

struct MediaPreparedRtpContainerStoragePlan final {
    std::size_t maximumPrefixObjects = 0;
    std::size_t maximumPendingLeases = 0;
    std::size_t outputPeakBytes = 0;
    std::size_t acquiringPeakBytes = 0;
    std::size_t prefixPeakBytes = 0;
    std::size_t pendingPeakBytes = 0;
    std::size_t pendingLeaseControlBytes = 0;
    std::size_t snapshotBytes = 0;
    std::size_t protocolPendingPeakBytes = 0;
    std::size_t protocolRequestPeakBytes = 0;
    std::size_t completionPeakBytes = 0;
    std::size_t aacHeaderPeakBytes = 0;
    // Candidate envelope only; other protocol owners are separate products.
    std::size_t peakBytes = 0;
};

} // namespace media::ffmpeg::graph
