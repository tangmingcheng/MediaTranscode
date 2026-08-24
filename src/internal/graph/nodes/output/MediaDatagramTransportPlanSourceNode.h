#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaDatagramTransportPlanSourceGenerationState final
    : public MediaProtocolOutputGenerationSessionState {
private:
    friend class MediaDatagramTransportPlanSourceNode;
    void resetForGenerationPurge() noexcept override
    {
        pendingPlan.reset();
        pendingGeneration.reset();
        publishedGeneration.reset();
    }

    MediaBufferRef pendingPlan;
    std::optional<std::uint64_t> pendingGeneration;
    std::optional<std::uint64_t> publishedGeneration;
};

class MediaDatagramTransportPlanSourceNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaDatagramTransportPlanSourceNode>>
    create(MediaNodeId nodeId,
           MediaDatagramTransportPlanTemplate planTemplate,
           std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
    static MediaNodeKind staticKind() noexcept;
    static constexpr std::string_view generationPurgeIdentity() noexcept
    {
        return "datagram_transport_plan_generation_state";
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
    MediaDatagramTransportPlanSourceNode(
        MediaNodeId nodeId,
        MediaDatagramTransportPlanTemplate planTemplate,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
    void resetState() noexcept;

    MediaDatagramTransportPlanTemplate m_planTemplate;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    std::shared_ptr<MediaDatagramTransportPlanSourceGenerationState>
        m_generationSession;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    MediaBufferRef& m_pendingPlan;
    std::optional<std::uint64_t>& m_pendingGeneration;
    std::optional<std::uint64_t>& m_publishedGeneration;
};

} // namespace media::ffmpeg::graph
