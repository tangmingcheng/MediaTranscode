#pragma once

#include "internal/graph/sync/MediaAvEpochTransitionService.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaAvSyncRuntimeBootstrap;

class MediaPlaybackEpochActivationCapability final {
public:
    MediaPlaybackEpochActivationCapability(
        MediaPlaybackEpochActivationCapability&&) noexcept = default;
    MediaPlaybackEpochActivationCapability& operator=(
        MediaPlaybackEpochActivationCapability&&) noexcept = default;
    MediaPlaybackEpochActivationCapability(
        const MediaPlaybackEpochActivationCapability&) = delete;
    MediaPlaybackEpochActivationCapability& operator=(
        const MediaPlaybackEpochActivationCapability&) = delete;

    ::media::Status activateInitial(MediaPlaybackEpoch epoch,
                                    MediaAudioPlaybackOrigin audioOrigin)
    {
        auto transition = m_transition.lock();
        return transition
            ? transition->activateInitial(epoch, audioOrigin)
            : ::media::Status::failure(::media::ErrorInfo::cancelled(
                  "Playback epoch activation capability has expired"));
    }

    ::media::Status activateNext(
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin,
        std::uint64_t completedTransitionSequence)
    {
        auto transition = m_transition.lock();
        return transition
            ? transition->activateNextAfter(completedTransitionSequence,
                                            epoch, audioOrigin)
            : ::media::Status::failure(::media::ErrorInfo::cancelled(
                  "Playback epoch activation capability has expired"));
    }

private:
    friend class MediaAvSyncRuntimeBootstrap;
    explicit MediaPlaybackEpochActivationCapability(
        std::weak_ptr<MediaAvEpochTransitionService> transition)
        : m_transition(std::move(transition))
    {
    }

    std::weak_ptr<MediaAvEpochTransitionService> m_transition;
};

} // namespace media::ffmpeg::graph
