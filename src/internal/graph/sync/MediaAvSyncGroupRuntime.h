#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaAvReacquisitionRequest.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime final {
public:
    enum class GenerationDisposition { Old, Current, ReacquisitionRequired };
    enum class LifecycleState { AwaitingEpoch, Active, ReacquisitionRequired, Aborted };
    static ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>> create(
        MediaAvSyncGroupKey key,
        MediaAvSyncPlan plan,
        std::shared_ptr<MediaMasterClock> clock);

    const MediaAvSyncGroupKey& key() const noexcept { return m_key; }
    const MediaAvSyncPlan& plan() const noexcept { return m_plan; }
    const std::shared_ptr<MediaMasterClock>& clock() const noexcept { return m_clock; }
    ::media::Status activatePlaybackEpoch(MediaPlaybackEpoch epoch);
    ::media::Status activateNextPlaybackEpoch(MediaPlaybackEpoch epoch);
    ::media::Result<MediaPlaybackEpoch> playbackEpoch() const;
    ::media::Result<GenerationDisposition> observeGeneration(
        std::uint64_t generation);
    ::media::Status requestReacquisition(MediaAvReacquisitionRequest request) noexcept;
    std::optional<MediaAvReacquisitionRequest> reacquisitionRequest() const noexcept;
    void markAborted() noexcept;
    LifecycleState lifecycleState() const noexcept;
    ::media::Result<MediaRunningTime> mapCanonicalToMaster(
        MediaRunningTime canonicalTime) const noexcept;

private:
    MediaAvSyncGroupRuntime(MediaAvSyncGroupKey key,
                            MediaAvSyncPlan plan,
                            std::shared_ptr<MediaMasterClock> clock);

    MediaAvSyncGroupKey m_key;
    MediaAvSyncPlan m_plan;
    std::shared_ptr<MediaMasterClock> m_clock;
    mutable std::mutex m_epochMutex;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAvReacquisitionRequest> m_reacquisitionRequest;
    LifecycleState m_lifecycleState = LifecycleState::AwaitingEpoch;
};

} // namespace media::ffmpeg::graph
