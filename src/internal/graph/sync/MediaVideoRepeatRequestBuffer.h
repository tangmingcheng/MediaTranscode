#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/sync/MediaVideoRepeatRequestId.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaVideoRepeatRequestBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaRunningTime canonicalPresentation,
        MediaRunningTime canonicalDuration,
        std::uint64_t generation,
        MediaVideoRepeatRequestId requestId);
    MediaBufferType type() const noexcept override;
    MediaRunningTime canonicalPresentation() const noexcept { return m_presentation; }
    MediaRunningTime canonicalDuration() const noexcept { return m_duration; }
    std::uint64_t generation() const noexcept { return m_generation; }
    MediaVideoRepeatRequestId requestId() const noexcept { return m_requestId; }
private:
    MediaVideoRepeatRequestBuffer(MediaRunningTime presentation,
                                  MediaRunningTime duration,
                                  std::uint64_t generation,
                                  MediaVideoRepeatRequestId requestId);
    MediaRunningTime m_presentation;
    MediaRunningTime m_duration;
    std::uint64_t m_generation;
    MediaVideoRepeatRequestId m_requestId;
};

} // namespace media::ffmpeg::graph
