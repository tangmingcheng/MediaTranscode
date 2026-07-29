#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAudioDriftServo.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaAudioDriftControllerState;
class MediaAvGenerationPurgeTarget;

class MediaAudioDriftControllerNode final : public FFmpegNodeRuntime {
public:
    MediaAudioDriftControllerNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey);
    ~MediaAudioDriftControllerNode() override;
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;

    static ::media::Result<MediaAudioDriftMeasurement>
    measureCanonicalPosition(
        const MediaAudioPlaybackOrigin& origin,
        MediaRunningTime sourceEndOnMaster,
        MediaCanonicalAudioSampleInterval projectedOutput,
        std::uint64_t sequence);

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status stage(const MediaBufferRef& audio);
    ::media::Result<bool> commitIfReady(MediaGraphExecutionContext& context);
    static void logDriftSample(
        const MediaAudioDriftMeasurement& measurement,
        const MediaAudioServoDecision& decision);
    void resetState() noexcept;

    MediaAvSyncGroupKey m_groupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::shared_ptr<MediaAudioDriftControllerState> m_state;
};

} // namespace media::ffmpeg::graph
