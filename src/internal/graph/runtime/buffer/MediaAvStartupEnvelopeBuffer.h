#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"

namespace media::ffmpeg::graph {

class MediaAvStartupEnvelopeBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(MediaBufferRef media,
                                                   MediaAvStartupAccessUnit unit,
                                                   MediaRunningTime observedAt);

    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept;
    const MediaAvStartupAccessUnit& unit() const noexcept;
    MediaRunningTime observedAt() const noexcept;

private:
    MediaAvStartupEnvelopeBuffer(MediaBufferRef media,
                                 MediaAvStartupAccessUnit unit,
                                 MediaRunningTime observedAt);
    MediaBufferRef m_media;
    MediaAvStartupAccessUnit m_unit;
    MediaRunningTime m_observedAt;
};

class MediaAvStartupClockBuffer final : public MediaBuffer {
public:
    explicit MediaAvStartupClockBuffer(MediaRunningTime masterNow);
    MediaBufferType type() const noexcept override;
    MediaRunningTime masterNow() const noexcept;

private:
    MediaRunningTime m_masterNow;
};

struct MediaAvReleasedUnit final {
    MediaBufferRef media;
    std::uint32_t trimLeadingSamples;
};

class MediaAvStartupReleaseBuffer final : public MediaBuffer {
public:
    MediaAvStartupReleaseBuffer(MediaPlaybackEpoch epoch,
                                std::vector<MediaAvReleasedUnit> video,
                                std::vector<MediaAvReleasedUnit> audio);

    MediaBufferType type() const noexcept override;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const std::vector<MediaAvReleasedUnit>& video() const noexcept;
    const std::vector<MediaAvReleasedUnit>& audio() const noexcept;

private:
    MediaPlaybackEpoch m_epoch;
    std::vector<MediaAvReleasedUnit> m_video;
    std::vector<MediaAvReleasedUnit> m_audio;
};

} // namespace media::ffmpeg::graph
