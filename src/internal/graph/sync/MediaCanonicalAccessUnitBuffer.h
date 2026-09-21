#pragma once

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
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval,
        std::vector<MediaCanonicalAudioContribution> audioContributions);

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
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept
    { return m_lineage; }
    const std::optional<MediaCanonicalAudioSampleInterval>&
    audioSampleInterval() const noexcept
    {
        return m_audioInterval;
    }

    const std::vector<MediaCanonicalAudioContribution>& audioContributions() const noexcept
    { return m_audioContributions; }

private:
    MediaCanonicalAccessUnitBuffer(
        MediaBufferRef media,
        MediaScheduledStream stream,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval,
        std::vector<MediaCanonicalAudioContribution> audioContributions);
    MediaBufferRef m_media;
    MediaScheduledStream m_stream;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
    std::optional<MediaCanonicalAudioSampleInterval> m_audioInterval;
    std::vector<MediaCanonicalAudioContribution> m_audioContributions;
};

} // namespace media::ffmpeg::graph
