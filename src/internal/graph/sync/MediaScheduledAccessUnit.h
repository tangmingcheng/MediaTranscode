#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaVideoRepeatRequestId.h"
#include "internal/graph/sync/MediaVideoSyncDecision.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaScheduledAccessUnitParameters final {
    MediaScheduledAccessUnitParameters() = delete;
    MediaScheduledAccessUnitParameters(
        MediaBufferRef media,
        MediaScheduledStream stream,
        MediaRunningTime canonicalPresentation,
        MediaRunningTime canonicalDispatch,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaRunningTime emitOnMaster,
        MediaRunningTime canonicalDuration,
        std::uint64_t generation,
        MediaCanonicalAccessUnitSequence canonicalSequence,
        std::optional<MediaCanonicalAccessUnitSequence> repeatedFrom,
        std::optional<MediaVideoRepeatRequestId> repeatRequestId,
        std::optional<MediaVideoSyncDecisionKind> videoDecision);

    MediaBufferRef media;
    MediaScheduledStream stream;
    MediaRunningTime canonicalPresentation;
    MediaRunningTime canonicalDispatch;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime dispatchOnMaster;
    MediaRunningTime emitOnMaster;
    MediaRunningTime canonicalDuration;
    std::uint64_t generation;
    MediaCanonicalAccessUnitSequence canonicalSequence;
    std::optional<MediaCanonicalAccessUnitSequence> repeatedFrom;
    std::optional<MediaVideoRepeatRequestId> repeatRequestId;
    std::optional<MediaVideoSyncDecisionKind> videoDecision;
};

class MediaScheduledAccessUnit final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaScheduledAccessUnitParameters parameters);
    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    MediaScheduledStream stream() const noexcept { return m_stream; }
    MediaRunningTime canonicalPresentation() const noexcept { return m_canonicalPresentation; }
    MediaRunningTime canonicalDispatch() const noexcept { return m_canonicalDispatch; }
    MediaRunningTime presentationOnMaster() const noexcept { return m_presentationOnMaster; }
    MediaRunningTime dispatchOnMaster() const noexcept { return m_dispatchOnMaster; }
    MediaRunningTime emitOnMaster() const noexcept { return m_emitOnMaster; }
    MediaRunningTime canonicalDuration() const noexcept { return m_canonicalDuration; }
    std::uint64_t generation() const noexcept { return m_generation; }
    MediaCanonicalAccessUnitSequence canonicalSequence() const noexcept { return m_canonicalSequence; }
    const std::optional<MediaCanonicalAccessUnitSequence>& repeatedFromCanonicalSequence() const noexcept
    {
        return m_repeatedFromCanonicalSequence;
    }
    const std::optional<MediaVideoRepeatRequestId>& repeatRequestId() const noexcept
    {
        return m_repeatRequestId;
    }
    const std::optional<MediaVideoSyncDecisionKind>& videoDecision() const noexcept
    {
        return m_videoDecision;
    }

private:
    explicit MediaScheduledAccessUnit(MediaScheduledAccessUnitParameters parameters);
    MediaBufferRef m_media;
    MediaScheduledStream m_stream;
    MediaRunningTime m_canonicalPresentation;
    MediaRunningTime m_canonicalDispatch;
    MediaRunningTime m_presentationOnMaster;
    MediaRunningTime m_dispatchOnMaster;
    MediaRunningTime m_emitOnMaster;
    MediaRunningTime m_canonicalDuration;
    std::uint64_t m_generation;
    MediaCanonicalAccessUnitSequence m_canonicalSequence;
    std::optional<MediaCanonicalAccessUnitSequence> m_repeatedFromCanonicalSequence;
    std::optional<MediaVideoRepeatRequestId> m_repeatRequestId;
    std::optional<MediaVideoSyncDecisionKind> m_videoDecision;
};

} // namespace media::ffmpeg::graph
