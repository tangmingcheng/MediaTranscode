#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <memory>

namespace media::ffmpeg::graph {

enum class MediaScheduledStream : std::uint8_t { Video, Audio };
enum class MediaDecodeOrderMode : std::uint8_t {
    ReorderedRequiresDecodeTime,
    PresentationOrderNoReorder
};

struct MediaCanonicalLineage;

class MediaSourceAccessUnitSequence final {
public:
    explicit constexpr MediaSourceAccessUnitSequence(std::uint64_t value) noexcept
        : m_value(value) {}
    constexpr std::uint64_t value() const noexcept { return m_value; }
    friend constexpr bool operator==(MediaSourceAccessUnitSequence,
                                     MediaSourceAccessUnitSequence) noexcept = default;
private:
    std::uint64_t m_value;
};

class MediaCanonicalAccessUnitBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval);

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
    MediaSourceAccessUnitSequence sourceSequence() const noexcept;
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept
    { return m_lineage; }
    const std::optional<MediaCanonicalAudioSampleInterval>&
    audioSampleInterval() const noexcept
    {
        return m_audioInterval;
    }

private:
    MediaCanonicalAccessUnitBuffer(
        MediaBufferRef media,
        MediaScheduledStream stream,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaCanonicalAudioSampleInterval> audioInterval);
    MediaBufferRef m_media;
    MediaScheduledStream m_stream;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
    std::optional<MediaCanonicalAudioSampleInterval> m_audioInterval;
};

} // namespace media::ffmpeg::graph
