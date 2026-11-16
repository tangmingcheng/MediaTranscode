#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaAvSourceCloseReason : std::uint8_t { SenderLeft, GroupClose };

// Frozen from the locked protocol member and its prepared input wait contract.
// All times here are absolute steady-clock nanoseconds, never master time.
struct MediaAvSourceDrainMember final {
    std::uint64_t ingressGeneration;
    std::uint32_t ssrc;
    MediaAvSourceCloseReason reason;
    std::int64_t observedAtNs;
    std::int64_t closeDeadlineNs;
    bool operator==(const MediaAvSourceDrainMember&) const = default;
};
struct MediaAvSourceDrainRequest final {
    std::uint64_t playbackGeneration;
    MediaAvSourceDrainMember video;
    std::optional<MediaAvSourceDrainMember> audio;
};
struct MediaAvSourceDrainMemberProgress final {
    std::optional<std::uint64_t> acceptedFrontier;
    bool drained = false;
};
struct MediaAvSourceDrainSnapshot final {
    MediaAvSourceDrainRequest request;
    MediaAvSourceDrainMemberProgress video;
    std::optional<MediaAvSourceDrainMemberProgress> audio;
    bool complete() const noexcept
    { return video.drained && (!audio || audio->drained); }
};
} // namespace media::ffmpeg::graph
