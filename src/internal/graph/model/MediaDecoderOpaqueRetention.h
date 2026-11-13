#pragma once
#include <cstdint>
#include <variant>
namespace media::ffmpeg::graph {
// Audited native FFmpeg containers of unique input opaque tokens.
struct MediaNativeDecoderOpaqueRetention final {
    int threadCount;
    int activeThreadType;
    std::uint64_t privateFrameSlots;
    std::uint64_t genericSlots;
    bool outputFifo;
    std::uint64_t frameWorkers;
    friend bool operator==(const MediaNativeDecoderOpaqueRetention&, const MediaNativeDecoderOpaqueRetention&) = default;
};
// Opened backend readback includes bridge, generic codec and asynchronous
// owners. It excludes only the engine's pending input and receive output.
struct MediaBackendDecoderOpaqueRetention final {
    int threadCount;
    int activeThreadType;
    std::uint64_t contractVersion;
    std::uint64_t retainedPackets;
    friend bool operator==(const MediaBackendDecoderOpaqueRetention&, const MediaBackendDecoderOpaqueRetention&) = default;
};
using MediaDecoderOpaqueRetention = std::variant<MediaNativeDecoderOpaqueRetention,
                                                MediaBackendDecoderOpaqueRetention>;
} // namespace media::ffmpeg::graph
