#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <optional>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

struct MediaCanonicalLineage;

class VideoFrameRateNode final : public FFmpegNodeRuntime {
public:
    explicit VideoFrameRateNode(MediaNodeId nodeId);
    VideoFrameRateNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaVideoFrameRateState> state,
        std::optional<MediaAvStartupVideoPreparationCapability> preparation = std::nullopt);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    friend struct VideoFrameRateNodeLifecycleTestAccess;

    ::media::Result<MediaNodeProcessResult> continueTerminal(MediaGraphExecutionContext& context);
    ::media::Status initializeFromFirstFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status drainPending(MediaGraphExecutionContext& context);
    ::media::Result<bool> preparePendingOutput(MediaGraphExecutionContext& context);
    ::media::Status queueFrameReference(
        const MediaBufferRef& sourceBuffer, int64_t outputPts,
        std::shared_ptr<const MediaCanonicalLineage> lineage);
    ::media::Status rememberLastInputFrame(const MediaBufferRef& buffer);

    ::media::Result<int64_t> targetPtsForIndex(int64_t outputIndex) const noexcept;
    int64_t targetFrameDuration() const noexcept;
    const AVFrame* chooseSourceFrameForTarget(const AVFrame* currentFrame,
                                              int64_t currentPts,
                                              int64_t targetPts) const noexcept;
    bool enabled() const noexcept;
    void resetRuntimeState() noexcept;

private:
    std::shared_ptr<MediaVideoFrameRateState> m_state;
    bool m_exposesGenerationPurgeTarget = false;
    bool m_firstInputDiagnosticEmitted = false;
    bool m_firstOutputDiagnosticEmitted = false;
    std::optional<MediaAvStartupVideoPreparationCapability> m_preparationCapability;
    std::optional<MediaReservedOutputTransaction> m_preparedReservation;
};

} // namespace media::ffmpeg::graph
