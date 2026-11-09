#pragma once
#include <cstdint>
namespace media::ffmpeg::graph {
// Version-scoped containers of unique input opaque tokens, not decoded frames.
struct MediaDecoderOpaqueRetention final {
    int threadCount;
    int activeThreadType;
    std::uint64_t privateFrameSlots;
    std::uint64_t genericSlots;
    bool outputFifo;
    std::uint64_t frameWorkers;
    friend bool operator==(const MediaDecoderOpaqueRetention&, const MediaDecoderOpaqueRetention&) = default;
};
}
