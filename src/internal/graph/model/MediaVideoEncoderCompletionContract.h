#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"
#include <string_view>
#include <array>
#include <cstdint>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaEncoderCompletionLifetime {
    UnchangedPreparedContextUntilReset
};

// Fixed-size opened-context readback; adapter owns field ordering. No media,
// vector/string allocation, or dependency ownership is retained here.
struct MediaNvencCompletionConfiguration final {
    std::int64_t delay, zeroLatency, lookahead;
    friend bool operator==(const MediaNvencCompletionConfiguration&,
                           const MediaNvencCompletionConfiguration&) = default;
};
struct MediaRkmppCompletionConfiguration final {
    std::int64_t known, splitMode, splitArgument, splitOutput, lowDelay;
    std::int64_t dropMode, inputFlexible, inputNumerator, inputDenominator;
    std::int64_t outputFlexible, outputNumerator, outputDenominator;
    friend bool operator==(const MediaRkmppCompletionConfiguration&,
                           const MediaRkmppCompletionConfiguration&) = default;
};
struct MediaVideoEncoderCompletionConfiguration final {
    std::array<std::int64_t, 22> fields;
    std::variant<MediaNvencCompletionConfiguration, MediaRkmppCompletionConfiguration> backend;
    friend bool operator==(const MediaVideoEncoderCompletionConfiguration&,
                           const MediaVideoEncoderCompletionConfiguration&) = default;
};

// A source-audited semantic contract, not a wall-clock completion bound.
// Each returned packet completes exactly one submitted progressive picture;
// packets preserve submission order and DTS equals that input's PTS. The
// admitted lineage transport identifies it; acceptance/EAGAIN prove no completion.
// Valid only on the captured prepared context while its configuration remains
// unchanged. Flush/reset/reopen revokes it; generation progress cannot survive
// that boundary. Consumers must enforce this lifetime before using a watermark.
struct MediaVideoEncoderCompletionContract final {
    MediaRational inputTimeBase;
    MediaRational inputCadence;
    MediaEncoderCompletionLifetime lifetime;
    MediaVideoLineagePropagation lineagePropagation;
    MediaVideoEncoderCompletionConfiguration configuration;
    // Implementation identity is checked by the audited dependency delivery
    // gate. This label documents source semantics; it is not a runtime hash.
    std::string_view authority;

    friend bool operator==(const MediaVideoEncoderCompletionContract&,
                           const MediaVideoEncoderCompletionContract&) = default;
};

} // namespace media::ffmpeg::graph
