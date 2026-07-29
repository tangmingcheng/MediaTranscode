#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvStartupReleaseKind.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>

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
    static ::media::Status validateReleaseKind(
        MediaAvStartupReleaseKind releaseKind) noexcept;
    static ::media::Result<MediaBufferRef> create(
        MediaAvSyncGroupKey groupKey,
        MediaAvStartupReleaseKind releaseKind,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin,
        std::vector<MediaAvReleasedUnit> video,
        std::vector<MediaAvReleasedUnit> audio,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    MediaAvStartupReleaseKind releaseKind() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;
    const std::vector<MediaAvReleasedUnit>& video() const noexcept;
    const std::vector<MediaAvReleasedUnit>& audio() const noexcept;
    const std::optional<std::uint64_t>&
    completedTransitionSequence() const noexcept;

private:
    MediaAvStartupReleaseBuffer(MediaAvSyncGroupKey groupKey,
                                MediaAvStartupReleaseKind releaseKind,
                                MediaPlaybackEpoch epoch,
                                MediaAudioPlaybackOrigin audioOrigin,
                                std::vector<MediaAvReleasedUnit> video,
                                std::vector<MediaAvReleasedUnit> audio,
                                std::optional<std::uint64_t>
                                    completedTransitionSequence);
    MediaAvSyncGroupKey m_groupKey;
    MediaAvStartupReleaseKind m_releaseKind;
    MediaPlaybackEpoch m_epoch;
    MediaAudioPlaybackOrigin m_audioOrigin;
    std::vector<MediaAvReleasedUnit> m_video;
    std::vector<MediaAvReleasedUnit> m_audio;
    std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph
