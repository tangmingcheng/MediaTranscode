#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include <string_view>
#include <array>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaEncoderCompletionLifetime {
    UnchangedPreparedContextUntilReset
};

// Fixed-size opened-context readback; adapter owns field ordering. No media,
// vector/string allocation, or dependency ownership is retained here.
struct MediaVideoEncoderCompletionConfiguration final {
    std::array<std::int64_t, 25> fields;
    friend bool operator==(const MediaVideoEncoderCompletionConfiguration&,
                           const MediaVideoEncoderCompletionConfiguration&) = default;
};

// A source-audited semantic contract, not a wall-clock completion bound.
// Each returned packet completes exactly one submitted progressive picture;
// packets preserve submission order, DTS equals that input's PTS, and opaque
// lineage identifies that picture. Acceptance and EAGAIN prove no completion.
// Valid only on the captured prepared context while its configuration remains
// unchanged. Flush/reset/reopen revokes it; generation progress cannot survive
// that boundary. Consumers must enforce this lifetime before using a watermark.
struct MediaVideoEncoderCompletionContract final {
    MediaRational inputTimeBase;
    MediaRational inputCadence;
    MediaEncoderCompletionLifetime lifetime;
    MediaVideoEncoderCompletionConfiguration configuration;
    // Implementation identity is checked by the audited dependency delivery
    // gate. This label documents source semantics; it is not a runtime hash.
    std::string_view authority;

    friend bool operator==(const MediaVideoEncoderCompletionContract&,
                           const MediaVideoEncoderCompletionContract&) = default;
};

} // namespace media::ffmpeg::graph
