#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>
#include <memory>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaAvGenerationPurgeTarget;
class MediaProtocolOutputGenerationState;

class MediaScheduledTsAccessUnitAdapterNode final : public FFmpegNodeRuntime {
public:
    MediaScheduledTsAccessUnitAdapterNode(MediaNodeId nodeId,
                                          MediaAvSyncGroupKey group);
    static MediaNodeKind staticKind() noexcept;
    static constexpr std::string_view generationPurgeIdentity() noexcept
    {
        return "scheduled_ts_adapter_generation_state";
    }
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Result<
        std::optional<MediaProtocolOutputGenerationCommitReservation>>
    reserveOutputCommit(const MediaBufferRef& buffer) const override;

private:
    void resetState() noexcept;
    ::media::Status validateOutputPermit(
        std::uint64_t generation) const;

    MediaAvSyncGroupKey m_group;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaRunningTime> m_transportLead;
    std::optional<std::uint64_t> m_pendingCommitGeneration;
};

} // namespace media::ffmpeg::graph
