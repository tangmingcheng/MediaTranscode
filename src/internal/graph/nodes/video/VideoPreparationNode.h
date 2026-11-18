#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/model/MediaVideoFrameLineageProduct.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/sync/lineage/MediaVideoLineageState.h"

namespace media::ffmpeg::graph {

// A source publication boundary. Owns references, never pixel allocations.
class VideoPreparationNode final : public FFmpegNodeRuntime {
public:
    VideoPreparationNode(MediaNodeId id, std::shared_ptr<MediaAvSyncGroupRuntime> group,
        MediaAvStartupVideoPreparationCapability preparation, MediaVideoFrameLineageProduct product);
    static MediaNodeKind staticKind() noexcept { return MediaNodeKind::VideoPreparation; }
    static std::string_view generationPurgeIdentity() noexcept { return "video_preparation"; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_purge; }
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef& buffer) const override;
private:
    ::media::Status servicePurge();
    void clear() noexcept;
    std::optional<std::uint64_t> generation(const MediaBufferRef& buffer) const noexcept;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    MediaAvStartupVideoPreparationCapability m_preparation;
    MediaVideoFrameLineageProduct m_product;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_purge;
    std::unique_ptr<MediaVideoLineageState> m_lineage;
    MediaBufferRef m_pending;
    std::optional<MediaSourceVideoObservationFacts> m_reservedObservation;
    std::optional<MediaReservedOutputTransaction> m_reservation;
    std::uint64_t m_generation = 0;
    std::uint64_t m_releaseIdentity = 0;
};

} // namespace media::ffmpeg::graph
