#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaScheduledStream : std::uint8_t { Video, Audio };
enum class MediaDecodeOrderMode : std::uint8_t {
    ReorderedRequiresDecodeTime,
    PresentationOrderNoReorder
};

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
        MediaScheduledStream stream,
        MediaRunningTime canonicalPresentation,
        std::optional<MediaRunningTime> canonicalDecode,
        MediaRunningTime canonicalDuration,
        MediaDecodeOrderMode decodeOrder,
        std::uint64_t generation,
        MediaSourceAccessUnitSequence sourceSequence);

    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    MediaScheduledStream stream() const noexcept { return m_stream; }
    MediaRunningTime canonicalPresentation() const noexcept { return m_canonicalPresentation; }
    const std::optional<MediaRunningTime>& canonicalDecode() const noexcept { return m_canonicalDecode; }
    ::media::Result<MediaRunningTime> canonicalDispatch() const noexcept;
    MediaRunningTime canonicalDuration() const noexcept { return m_canonicalDuration; }
    MediaDecodeOrderMode decodeOrder() const noexcept { return m_decodeOrder; }
    std::uint64_t generation() const noexcept { return m_generation; }
    MediaSourceAccessUnitSequence sourceSequence() const noexcept { return m_sourceSequence; }

private:
    MediaCanonicalAccessUnitBuffer(MediaBufferRef media,
                                   MediaScheduledStream stream,
                                   MediaRunningTime canonicalPresentation,
                                   std::optional<MediaRunningTime> canonicalDecode,
                                   MediaRunningTime canonicalDuration,
                                   MediaDecodeOrderMode decodeOrder,
                                   std::uint64_t generation,
                                   MediaSourceAccessUnitSequence sourceSequence);
    MediaBufferRef m_media;
    MediaScheduledStream m_stream;
    MediaRunningTime m_canonicalPresentation;
    std::optional<MediaRunningTime> m_canonicalDecode;
    MediaRunningTime m_canonicalDuration;
    MediaDecodeOrderMode m_decodeOrder;
    std::uint64_t m_generation;
    MediaSourceAccessUnitSequence m_sourceSequence;
};

} // namespace media::ffmpeg::graph
