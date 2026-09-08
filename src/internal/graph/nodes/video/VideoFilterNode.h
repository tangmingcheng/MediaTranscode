#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaVideoLineageState.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadReservation.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class MediaGraphPayloadCreditLease;

class VideoFilterLineageState final : public MediaVideoLineageState {
public:
    explicit VideoFilterLineageState(
        std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept;

    void resetFilterGraph() noexcept;
    void resetForLifecycle() noexcept;

    MediaBufferRef encoderConfig;
    AVCodecContext* encoderContext = nullptr;
    ::media::ffmpeg::FilterGraphPtr filterGraph;
    AVFilterContext* bufferSrcContext = nullptr;
    AVFilterContext* bufferSinkContext = nullptr;
    AVRational inputTimeBase { 0, 1 };
    AVRational sinkTimeBase { 0, 1 };
    int64_t lastSubmittedPts = AV_NOPTS_VALUE;
    bool graphInitialized = false;
    bool flushed = false;
    bool filterEof = false;
    ::media::ffmpeg::FramePtr pendingFrame;
    std::shared_ptr<MediaGraphPayloadCreditLease> pendingPayloadCredit;
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    MediaInputTerminalTracker terminals { { "frame" } };
    bool eofEmitted = false;
    MediaBufferRef terminalBuffer;
    bool terminalPending = false;
    bool terminalIsEof = false;
    std::set<std::uint64_t> lineageGenerations;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearGenerationLineage() noexcept;
    void clearLineageStorage() noexcept;
};

class VideoFilterNode final : public FFmpegNodeRuntime {
public:
    explicit VideoFilterNode(MediaNodeId nodeId);
    VideoFilterNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry);
    VideoFilterNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
        MediaAvStartupVideoPreparationCapability preparationCapability);
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
    friend struct VideoFilterNodeLifecycleTestAccess;

    ::media::Status bindEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status initializeGraph(MediaGraphExecutionContext& context, const MediaBufferRef& firstFrameBuffer);
    ::media::Status sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status submitPendingFrame(MediaGraphExecutionContext& context);
    ::media::Status attachPendingLineage();
    ::media::Status flushGraph(MediaGraphExecutionContext& context);
    ::media::Status drainFrames(MediaGraphExecutionContext& context, bool* produced = nullptr);
    ::media::Status emitFrame(
        MediaGraphExecutionContext& context,
        ::media::ffmpeg::FramePtr frame,
        MediaGraphPayloadReservation reservation);
    ::media::Result<MediaNodeProcessResult> continueTerminal(MediaGraphExecutionContext& context);
    ::media::Status rescaleAndValidateFrame(AVFrame* frame) noexcept;
    ::media::Status retainPreparedOutput(
        MediaBufferRef output,
        std::uint64_t generation,
        std::uint64_t releaseIdentity);
    ::media::Status markPreparedReadyOutsideLineageLock(
        MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;

private:
    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::shared_ptr<VideoFilterLineageState> m_lineageState;
    std::optional<MediaAvStartupVideoPreparationCapability>
        m_preparationCapability;
    MediaBufferRef m_preparedOutput;
    std::optional<MediaReservedOutputTransaction> m_preparedReservation;
    std::uint64_t m_preparedGeneration = 0;
    std::uint64_t m_preparedReleaseIdentity = 0;
    bool m_preparedNeedsReady = false;
    bool m_preparationFeedArmed = false;
    bool m_firstInputDiagnosticEmitted = false;
    bool m_firstOutputDiagnosticEmitted = false;
    std::optional<MediaHardwareDescriptor> m_inputContract;
    std::optional<MediaHardwareDescriptor> m_outputContract;
    std::uint64_t m_drmPrimeInputFrames = 0;
    std::uint64_t m_drmPrimeOutputFrames = 0;
    std::uint64_t m_rgaFrames = 0;
    std::uint64_t m_softwareFrames = 0;
    MediaVideoFilterImplementation m_filterImplementation =
        MediaVideoFilterImplementation::Unknown;
};

} // namespace media::ffmpeg::graph
