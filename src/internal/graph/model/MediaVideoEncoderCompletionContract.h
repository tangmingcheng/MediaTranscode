#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaEncoderCompletionLifetime {
    UnchangedPreparedContextUntilReset
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
    // The enclosing MediaVideoEncoderReadback stores the actual public-field
    // snapshot. This requirement is deliberately not a runtime authorization.
    MediaEncoderCompletionLifetime lifetime;
    // Implementation identity is checked by the audited dependency delivery
    // gate. This label documents source semantics; it is not a runtime hash.
    std::string_view authority;

    friend bool operator==(const MediaVideoEncoderCompletionContract&,
                           const MediaVideoEncoderCompletionContract&) = default;
};

} // namespace media::ffmpeg::graph
