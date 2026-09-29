#pragma once

#include "internal/graph/sync/MediaAvGenerationTransition.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget {
public:
    virtual ~MediaAvGenerationPurgeTarget() = default;
    // WouldBlock means this exact transaction is still owned by the target.
    // The caller may poll it, but must not acknowledge it until success.
    virtual ::media::Status purge(const MediaAvGenerationPurge& purge) = 0;
    void bindPurgeProgressWakeups(
        std::shared_ptr<const std::vector<std::shared_ptr<MediaNodeWakeup>>> wakeups)
    {
        m_progressWakeups = std::move(wakeups);
    }

protected:
    void notifyPurgeProgress() const noexcept
    {
        if (m_progressWakeups) {
            for (const auto& wakeup : *m_progressWakeups) wakeup->notify();
        }
    }

private:
    std::shared_ptr<const std::vector<std::shared_ptr<MediaNodeWakeup>>> m_progressWakeups;
};

} // namespace media::ffmpeg::graph
