#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"

#include <mutex>
#include <optional>
#include <stop_token>

namespace media::ffmpeg::graph {

// One bounded request, serviced by the node owner before normal media work.
// Completion is durable until a strictly newer transaction replaces it.
class MediaOwnerThreadGenerationPurge final : public MediaAvGenerationPurgeTarget {
public:
    ::media::Status start(std::shared_ptr<MediaNodeWakeup> ownerWakeup);
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    std::optional<MediaAvGenerationPurge> pending() const;
    ::media::Status complete(const MediaAvGenerationPurge& purge, ::media::Status result);
    std::stop_token waitStopToken() const;
    void stop() noexcept;

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<MediaNodeWakeup> m_ownerWakeup;
    std::optional<MediaAvGenerationPurge> m_request;
    std::optional<::media::Status> m_result;
    std::stop_source m_waitStop;
    bool m_stopped = true;
};

} // namespace media::ffmpeg::graph
