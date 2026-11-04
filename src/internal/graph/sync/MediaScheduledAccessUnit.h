#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

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
        std::optional<MediaCodecReceiveObservation> codecReceiveObservation);

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
    std::optional<MediaCodecReceiveObservation> codecReceiveObservation;
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
    const std::optional<MediaCodecReceiveObservation>& codecReceiveObservation() const noexcept { return m_codecReceiveObservation; }
    std::optional<std::int64_t> diagnosticCreatedSteadyNs() const noexcept { return m_diagnosticCreatedSteadyNs; }

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
    const std::optional<std::int64_t> m_diagnosticCreatedSteadyNs;
    const std::optional<MediaCodecReceiveObservation> m_codecReceiveObservation;
};

} // namespace media::ffmpeg::graph
