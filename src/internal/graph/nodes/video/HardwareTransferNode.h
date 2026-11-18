#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/runtime/resource/MediaSourceReferenceAllocation.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/lineage/MediaVideoReferenceLineageState.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

extern "C" {
#include <libavutil/frame.h>
}

#include <cstdint>

namespace media::ffmpeg::graph {

class HardwareTransferNode final : public FFmpegNodeRuntime, public MediaSourceReferenceAllocation {
public:
    explicit HardwareTransferNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status bindSourceGroup(std::shared_ptr<MediaAvSyncGroupRuntime> group,
        std::shared_ptr<MediaAvStartupVideoPreparationState> preparation);
    static std::string_view generationPurgeIdentity() noexcept { return "hardware_transfer"; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_purge; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

protected:
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef& buffer) const override;

private:
    enum class Direction {
        None,
        Download,
        Upload,
        Map,
        Unmap
    };

    void resetRuntimeState() noexcept;
    ::media::Status servicePurge();
    std::optional<std::uint64_t> generation(const MediaBufferRef& buffer) const noexcept;
    void logSummary() const;
    ::media::Status emitTracedOutput(MediaGraphExecutionContext& context,
                                     const MediaBufferRef& buffer);
    ::media::Status transferOrForward(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status downloadHardwareFrame(MediaGraphExecutionContext& context,
                                          const MediaBufferRef& buffer,
                                          const AVFrame* sourceFrame);

    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    bool m_firstInputDiagnosticEmitted = false;
    bool m_firstOutputDiagnosticEmitted = false;
    Direction m_direction = Direction::None;
    MediaBufferRef m_pendingInput;
    std::uint64_t m_forwardedFrames = 0;
    std::uint64_t m_downloads = 0;
    std::uint64_t m_uploads = 0;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::shared_ptr<MediaAvStartupVideoPreparationState> m_preparation;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_purge;
    std::unique_ptr<MediaVideoReferenceLineageState> m_lineage;
    bool m_bindingFrozen = false;
    bool m_generationEnded = false;
    std::uint64_t m_generation = 0;
    std::uint64_t m_releaseIdentity = 0;
};

} // namespace media::ffmpeg::graph
