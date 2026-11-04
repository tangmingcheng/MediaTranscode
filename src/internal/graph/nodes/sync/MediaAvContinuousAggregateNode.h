#pragma once
#include "internal/graph/diagnostics/MediaAudioStageObservation.h"
#include "internal/graph/diagnostics/MediaAggregateExecutionObservation.h"

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/runtime/factory/MediaAvAggregateRuntimeDependencies.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasProducer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"

#include "internal/graph/runtime/storage/MediaAvAggregateStorage.h"

namespace media::ffmpeg::graph {

// Video owner selects source frames and commits the output frame grid.
// Source purge is serviced before output backpressure; audio has its own owner.
class MediaAvContinuousAggregateNode final : public FFmpegNodeRuntime {
public:
    MediaAvContinuousAggregateNode(MediaNodeId node,
                                  MediaAvAggregateRuntimeDependencies dependencies);
    static MediaNodeKind staticKind() noexcept { return MediaNodeKind::AvContinuousAggregate; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> sourcePurgeTarget(
        const MediaAvSyncGroupKey& group) const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(
        const MediaBufferRef& buffer) const override;
    ::media::Status commitReservedOutput(const MediaBufferRef& buffer) override;
    ::media::Status cancelReservedOutput(const MediaBufferRef& buffer) override;

private:
    using ExecutionStage = MediaAggregateExecutionObservation::Stage;
    using SourceState = MediaAvAggregateSourceStorage;
    using PendingKind = MediaAvAggregatePendingKind;
    using Pending = MediaAvAggregatePendingStorage;
    const MediaAvContinuousAggregatePlan& plan() const noexcept;
    ::media::Status bindCodecs(MediaGraphExecutionContext& context);
    ::media::Status servicePurges(MediaGraphExecutionContext& context);
    enum class CandidatePreparation { Ready, RetryProgress };
    ::media::Status consumeInputs(MediaGraphExecutionContext& context,
                                  CandidatePreparation& preparation);
    ::media::Result<bool> activate(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> buildVideo(MediaGraphExecutionContext& context);
    void clear() noexcept;
    void logSummary(const char* reason) noexcept;

    MediaAvAggregateRuntimeDependencies m_dependencies;
    MediaFixedCapacityStorage<SourceState> m_sources;
    MediaBufferRef m_videoCodec;
    std::unique_ptr<MediaVideoCanvasProducer> m_canvas;
    bool m_canvasPrepared = false;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::int64_t m_nextVideoFrame = 0;
    std::optional<Pending> m_pending;
    bool m_videoEndPublished = false;
    mutable bool m_rejectedPending = false;
    bool m_summaryPublished = false;
    MediaAggregateExecutionObservation m_executionObservation;
};

} // namespace media::ffmpeg::graph
