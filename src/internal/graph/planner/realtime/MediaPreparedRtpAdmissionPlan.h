#pragma once

#include <cstddef>
#include <optional>

namespace media::ffmpeg::graph {

// Deployment byte capacity bounds actual reservations; the reference-count
// ceiling only prevents unbounded bookkeeping, not a promise to fit every AU.
struct MediaPreparedRtpAdmissionStream final {
    std::size_t maximumUnits;
    std::size_t maximumBytes;
};
struct MediaPreparedRtpAdmissionPlan final {
    std::size_t sharedCapacityBytes;
    MediaPreparedRtpAdmissionStream video;
    std::optional<MediaPreparedRtpAdmissionStream> audio;
};

} // namespace media::ffmpeg::graph
