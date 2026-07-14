#pragma once

#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRegistry final {
public:
    MediaAvSyncGroupRegistry();
    ~MediaAvSyncGroupRegistry();
    MediaAvSyncGroupRegistry(const MediaAvSyncGroupRegistry&) = delete;
    MediaAvSyncGroupRegistry& operator=(const MediaAvSyncGroupRegistry&) = delete;
    MediaAvSyncGroupRegistry(MediaAvSyncGroupRegistry&& other);
    MediaAvSyncGroupRegistry& operator=(MediaAvSyncGroupRegistry&& other);

    ::media::Status registerGroup(MediaAvSyncGroupKey key,
                                  MediaAvSyncPlan plan,
                                  std::shared_ptr<MediaMasterClock> clock);
    ::media::Status activatePlaybackEpoch(const MediaAvSyncGroupKey& key,
                                          MediaPlaybackEpoch epoch);
    ::media::Status activateNextPlaybackEpoch(const MediaAvSyncGroupKey& key,
                                              MediaPlaybackEpoch epoch);
    std::shared_ptr<MediaAvSyncGroupRuntime> find(
        const MediaAvSyncGroupKey& key) const noexcept;
    void clear() noexcept;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace media::ffmpeg::graph
