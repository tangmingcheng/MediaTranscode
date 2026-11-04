#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

#include "internal/graph/runtime/storage/MediaImmutableArray.h"
#include "internal/graph/diagnostics/MediaCodecReceiveObservation.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAudioContribution.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitIdentity.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaCanonicalLineage;

class MediaCanonicalAccessUnitBuffer final : public MediaBuffer {
    class ConstructionKey final {
        friend class MediaCanonicalAccessUnitBuffer;
        ConstructionKey() = default;
    };
public:
    using AllocationAllocator = MediaLineageAllocator<MediaCanonicalAccessUnitBuffer>;
    static ::media::Result<MediaBufferRef> create(
        const MediaLineageAllocationScope& allocation,
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval,
        std::vector<MediaCanonicalAudioContribution> audioContributions,
        std::optional<MediaCodecReceiveObservation> codecReceiveObservation);

    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    MediaScheduledStream stream() const noexcept { return m_stream; }
    MediaRunningTime canonicalPresentation() const noexcept;
    const std::optional<MediaRunningTime>& canonicalDecode() const noexcept;
    ::media::Result<MediaRunningTime> canonicalDispatch() const noexcept;
    MediaRunningTime canonicalDuration() const noexcept;
    MediaDecodeOrderMode decodeOrder() const noexcept;
    std::uint64_t generation() const noexcept;
    MediaCanonicalAccessUnitSequence canonicalSequence() const noexcept;
    // Construction observation, not a successful output-publication timestamp.
    std::optional<std::int64_t> diagnosticCreatedSteadyNs() const noexcept
    { return m_diagnosticCreatedSteadyNs; }
    const std::optional<MediaCodecReceiveObservation>& codecReceiveObservation() const noexcept
    { return m_codecReceiveObservation; }
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept
    { return m_lineage; }
    const std::optional<MediaCanonicalAudioSampleInterval>&
    audioSampleInterval() const noexcept
    {
        return m_audioInterval;
    }

    std::span<const MediaCanonicalAudioContribution> audioContributions() const noexcept
    { return m_audioContributions.view(); }

    MediaCanonicalAccessUnitBuffer(ConstructionKey,
        MediaBufferRef media,
        MediaScheduledStream stream,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval,
        MediaImmutableArray<MediaCanonicalAudioContribution> audioContributions,
        std::optional<MediaCodecReceiveObservation> codecReceiveObservation);
private:
    MediaBufferRef m_media;
    MediaScheduledStream m_stream;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
    std::optional<MediaCanonicalAudioSampleInterval> m_audioInterval;
    MediaImmutableArray<MediaCanonicalAudioContribution> m_audioContributions;
    const std::optional<std::int64_t> m_diagnosticCreatedSteadyNs;
    const std::optional<MediaCodecReceiveObservation> m_codecReceiveObservation;
};

} // namespace media::ffmpeg::graph
