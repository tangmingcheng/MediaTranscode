#pragma once
#include "internal/graph/diagnostics/MediaAvHeadWaitObservation.h"

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

enum class MediaAvSchedulerHeadKind { Canonical, Control };

class MediaAvSchedulerHead final {
public:
    static ::media::Result<MediaAvSchedulerHead> parse(
        MediaBufferRef buffer, MediaScheduledStream expectedStream);

    void recordArrival(std::optional<MediaRunningTime> arrival) noexcept { m_arrival = arrival; }
    std::optional<MediaRunningTime> arrival() const noexcept { return m_arrival; }
    MediaAvHeadWaitObservation& waitObservation() noexcept { return m_waitObservation; }
    const MediaAvHeadWaitObservation& waitObservation() const noexcept { return m_waitObservation; }
    MediaAvSchedulerHeadKind kind() const noexcept { return m_kind; }
    const MediaBufferRef& buffer() const noexcept { return m_buffer; }
    const MediaCanonicalAccessUnitBuffer* canonical() const noexcept;
    std::uint64_t generation() const noexcept;
    MediaRunningTime canonicalPresentation() const noexcept;
    ::media::Result<MediaRunningTime> canonicalDispatchTime() const noexcept;

private:
    MediaAvSchedulerHead(MediaBufferRef buffer, MediaAvSchedulerHeadKind kind)
        : m_buffer(std::move(buffer)), m_kind(kind) {}
    MediaBufferRef m_buffer;
    MediaAvSchedulerHeadKind m_kind;
    std::optional<MediaRunningTime> m_arrival;
    MediaAvHeadWaitObservation m_waitObservation;
};

} // namespace media::ffmpeg::graph
