#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRetainedControlFreshness final {
public:
    static bool isCandidate(const MediaBufferRef& buffer) noexcept;
    ::media::Status capture(
        const MediaBufferRef& buffer,
        std::uint64_t generation,
        MediaStreamKind expectedStream);
    bool matches(
        const MediaBufferRef& buffer,
        std::uint64_t generation,
        MediaStreamKind expectedStream) const noexcept;
    void clear() noexcept;

private:
    MediaBufferWeakRef m_buffer;
    std::uint64_t m_generation = 0;
};

} // namespace media::ffmpeg::graph
