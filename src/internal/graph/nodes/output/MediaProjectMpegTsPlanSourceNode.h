#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <cstdint>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget;
class MediaAvSyncGroupRuntime;
class MediaProjectMpegTsPlanSourceGenerationState final
    : public MediaProtocolOutputGenerationSessionState {
private:
    friend class MediaProjectMpegTsPlanSourceNode;
    void resetForGenerationPurge() noexcept override
    {
        pendingPlan.reset();
        publishedGeneration.reset();
        published = false;
    }

    MediaBufferRef pendingPlan;
    std::optional<std::uint64_t> publishedGeneration;
    bool published = false;
};

class MediaProjectMpegTsPlanSourceNode final : public FFmpegNodeRuntime {
public:
    MediaProjectMpegTsPlanSourceNode(MediaNodeId nodeId,
                                     MediaAvSyncGroupKey group,
                                     MediaProjectMpegTsRuntimeOutputPlan
                                         outputPlan);
    static MediaNodeKind staticKind() noexcept;
    static constexpr std::string_view generationPurgeIdentity() noexcept
    {
        return "project_mpegts_output_generation_state";
    }
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Result<MediaOutputCommitReservation>
    reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    void resetState() noexcept;

    MediaAvSyncGroupKey m_group;
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> m_outputPlan;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::shared_ptr<MediaProjectMpegTsPlanSourceGenerationState>
        m_generationSession;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    MediaBufferRef& m_pendingPlan;
    std::optional<std::uint64_t>& m_publishedGeneration;
    bool& m_published;
};

} // namespace media::ffmpeg::graph
