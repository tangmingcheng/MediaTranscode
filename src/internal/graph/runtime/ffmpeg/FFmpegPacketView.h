#pragma once

#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

struct MediaCanonicalLineage;

class FFmpegPacketView final {
public:
    static AVPacket* writablePacket(const MediaBufferRef& buffer) noexcept;
    static const AVPacket* packet(const MediaBufferRef& buffer) noexcept;
    static bool isPacket(const MediaBufferRef& buffer) noexcept;
    static std::shared_ptr<const MediaCanonicalLineage> canonicalLineage(
        const MediaBufferRef& buffer) noexcept;
};

} // namespace media::ffmpeg::graph
