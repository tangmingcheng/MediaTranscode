#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/diagnostics/MediaAudioStageObservation.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/storage/MediaAvAggregateStorage.h"
#include "internal/graph/sync/MediaAvAggregateCoordination.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {

struct MediaContinuousAudioAggregateDependencies final {
    std::shared_ptr<MediaAvSyncGroupRuntime> source;
    std::shared_ptr<MediaAvSyncGroupRuntime> output;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    std::shared_ptr<MediaAvAggregateCoordination> coordination;
};

class MediaContinuousAudioAggregateNode final : public FFmpegNodeRuntime {
public:
    MediaContinuousAudioAggregateNode(MediaNodeId node, MediaContinuousAudioAggregateDependencies dependencies);
    static MediaNodeKind staticKind() noexcept { return MediaNodeKind::ContinuousAudioAggregate; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> sourcePurgeTarget() const noexcept { return m_purge; }
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef&) const override;
    ::media::Status commitReservedOutput(const MediaBufferRef& buffer) override;
    ::media::Status cancelReservedOutput(const MediaBufferRef&) override;

private:
    using PendingKind = MediaAvAggregatePendingKind;
    using Pending = MediaAvAggregatePendingStorage;
    const MediaAvContinuousAggregatePlan& plan() const noexcept { return *m_dependencies.aggregatePlan; }
    ::media::Status consumeInputs(MediaGraphExecutionContext& context);
    ::media::Status servicePurge(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> buildAudio(MediaGraphExecutionContext& context);
    ::media::Result<std::int64_t> audioOffset(const MediaAudioPlaybackOrigin& source) const;
    void clear() noexcept;
    void logSummary(const char* reason) noexcept;

    MediaContinuousAudioAggregateDependencies m_dependencies;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_purge;
    MediaFixedCapacityStorage<std::shared_ptr<MediaBoundCanonicalAudioBuffer>> m_audio;
    std::int64_t m_audioCandidateSamples = 0;
    std::uint64_t m_minimumGeneration = 0;
    bool m_audioEnded = false;
    MediaBufferRef m_audioCodec;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
    std::int64_t m_nextAudioSample = 0;
    std::optional<Pending> m_pending;
    bool m_audioEndPublished = false;
    mutable bool m_rejectedPending = false;
    std::uint64_t m_committedAudioFrames = 0;
    std::uint64_t m_committedRealSamples = 0;
    std::uint64_t m_committedSilenceSamples = 0;
    bool m_summaryPublished = false;
    MediaAudioStageObservation m_audioBuiltObservation;
    MediaAudioStageObservation m_audioCommitObservation;
};

} // namespace media::ffmpeg::graph
