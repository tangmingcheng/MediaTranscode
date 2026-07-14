#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaVideoRepeatRequestBuffer.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

enum class MediaAvSchedulerHeadKind { Canonical, VideoRepeat, Control };

class MediaAvSchedulerHead final {
public:
    static ::media::Result<MediaAvSchedulerHead> parse(
        MediaBufferRef buffer, MediaScheduledStream expectedStream);

    MediaAvSchedulerHeadKind kind() const noexcept { return m_kind; }
    const MediaBufferRef& buffer() const noexcept { return m_buffer; }
    const MediaCanonicalAccessUnitBuffer* canonical() const noexcept;
    const MediaVideoRepeatRequestBuffer* repeat() const noexcept;
    std::uint64_t generation() const noexcept;
    MediaRunningTime canonicalPresentation() const noexcept;
    ::media::Result<MediaRunningTime> canonicalDispatchTime() const noexcept;

private:
    MediaAvSchedulerHead(MediaBufferRef buffer, MediaAvSchedulerHeadKind kind)
        : m_buffer(std::move(buffer)), m_kind(kind) {}
    MediaBufferRef m_buffer;
    MediaAvSchedulerHeadKind m_kind;
};

} // namespace media::ffmpeg::graph
