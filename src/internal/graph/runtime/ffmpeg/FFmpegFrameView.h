#pragma once

#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

struct MediaCanonicalLineage;

class FFmpegFrameView final {
public:
    static AVFrame* writableFrame(const MediaBufferRef& buffer) noexcept;
    static const AVFrame* frame(const MediaBufferRef& buffer) noexcept;
    static bool isFrame(const MediaBufferRef& buffer) noexcept;
    static std::shared_ptr<const MediaCanonicalLineage> canonicalLineage(
        const MediaBufferRef& buffer) noexcept;
    static const std::shared_ptr<MediaGraphPayloadCreditLease>& payloadCredit(
        const MediaBufferRef& buffer) noexcept;
};

} // namespace media::ffmpeg::graph
