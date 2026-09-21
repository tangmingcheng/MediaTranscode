#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/runtime/factory/MediaAvAggregateRuntimeDependencies.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasProducer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"

#include <deque>

namespace media::ffmpeg::graph {

// One owner selects all source candidates and commits the independent output
// sample/frame grids. Source purge is serviced before output backpressure.
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
    struct SourceState final {
        std::deque<MediaBufferRef> video;
        std::shared_ptr<MediaOwnerThreadGenerationPurge> purge;
        std::uint64_t minimumGeneration = 0;
        bool videoEnded = false;
        bool epochEnded = false;
        bool discardedAudioEnded = false;
        bool prepared = false;
        std::optional<MediaRunningTime> lastInputEnd;
        std::uint64_t committedRealFrames = 0;
        std::uint64_t committedBlackFrames = 0;
        std::uint64_t committedGeneration = 0;
    };
    enum class PendingKind { Video, Audio, VideoEnd, AudioEnd };
    struct Pending final {
        PendingKind kind;
        MediaBufferRef buffer;
        std::vector<std::uint64_t> sourceGenerations;
    };
    const MediaAvContinuousAggregatePlan& plan() const noexcept;
    ::media::Status bindCodecs(MediaGraphExecutionContext& context);
    ::media::Status servicePurges(MediaGraphExecutionContext& context);
    ::media::Status consumeInputs(MediaGraphExecutionContext& context);
    ::media::Result<bool> activate(MediaGraphExecutionContext& context);
    ::media::Result<MediaRunningTime> videoTime(std::int64_t frame) const;
    ::media::Result<MediaRunningTime> audioTime(std::int64_t sample) const;
    ::media::Result<MediaRunningTime> presentationMaster(MediaRunningTime presentation) const;
    ::media::Result<MediaBufferRef> buildVideo(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> buildAudio(MediaGraphExecutionContext& context);
    ::media::Result<std::int64_t> audioOffset(const MediaAudioPlaybackOrigin& source) const;
    bool allSourcesEnded() const noexcept;
    ::media::Status observeInputEnd(std::size_t source, MediaRunningTime end);
    ::media::Status validateMetadataBound() const;
    void clear() noexcept;
    void logSummary(const char* reason) noexcept;

    MediaAvAggregateRuntimeDependencies m_dependencies;
    std::vector<SourceState> m_sources;
    std::deque<std::shared_ptr<MediaBoundCanonicalAudioBuffer>> m_audio;
    std::int64_t m_audioCandidateSamples = 0;
    bool m_audioEnded = false;
    MediaBufferRef m_videoCodec;
    MediaBufferRef m_audioCodec;
    MediaVideoCanvasProducer m_canvas;
    bool m_canvasPrepared = false;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
    std::int64_t m_nextVideoFrame = 0;
    std::int64_t m_nextAudioSample = 0;
    std::optional<MediaRunningTime> m_lastInputEnd;
    std::optional<Pending> m_pending;
    bool m_videoEndPublished = false;
    bool m_audioEndPublished = false;
    mutable bool m_rejectedPending = false;
    std::uint64_t m_committedAudioFrames = 0;
    std::uint64_t m_committedRealSamples = 0;
    std::uint64_t m_committedSilenceSamples = 0;
    bool m_summaryPublished = false;
};

} // namespace media::ffmpeg::graph
