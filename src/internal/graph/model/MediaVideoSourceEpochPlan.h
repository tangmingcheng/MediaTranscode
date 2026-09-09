#pragma once

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {
// Playback ownership epoch. RTP clock evidence generations are a separate
// domain and may legitimately start at zero.
enum class MediaVideoSourceEpochAuthority { ProtocolSession, CanonicalLineage };
enum class MediaVideoSourceIdentityTransition { ReplanSession };
struct MediaVideoSourceEpochPlan final {
    MediaVideoSourceEpochAuthority authority;
    std::optional<std::uint64_t> protocolSessionEpoch;
    MediaVideoSourceIdentityTransition identityTransition;
};
} // namespace media::ffmpeg::graph
