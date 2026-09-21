#pragma once

#include "internal/graph/sync/MediaAvEpochTransitionService.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaAvSyncRuntimeBootstrap;

class MediaOutputEpochActivationCapability final {
public:
    MediaOutputEpochActivationCapability(
        MediaOutputEpochActivationCapability&&) noexcept = default;
    MediaOutputEpochActivationCapability& operator=(
        MediaOutputEpochActivationCapability&&) noexcept = default;
    MediaOutputEpochActivationCapability(
        const MediaOutputEpochActivationCapability&) = delete;
    MediaOutputEpochActivationCapability& operator=(
        const MediaOutputEpochActivationCapability&) = delete;

    ::media::Status activateInitial(MediaPlaybackEpoch epoch,
                                    MediaAudioPlaybackOrigin audioOrigin)
    {
        auto service = m_service.lock();
        return service
            ? service->activateInitial(epoch, audioOrigin)
            : ::media::Status::failure(::media::ErrorInfo::cancelled(
                  "Output epoch activation capability has expired"));
    }

private:
    friend class MediaAvSyncRuntimeBootstrap;
    explicit MediaOutputEpochActivationCapability(
        std::weak_ptr<MediaAvEpochTransitionService> service)
        : m_service(std::move(service))
    {
    }

    std::weak_ptr<MediaAvEpochTransitionService> m_service;
};

} // namespace media::ffmpeg::graph
