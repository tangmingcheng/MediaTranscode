#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

extern "C" {
#include <libavutil/frame.h>
}

#include <cstdint>

namespace media::ffmpeg::graph {

class HardwareTransferNode final : public FFmpegNodeRuntime {
public:
    explicit HardwareTransferNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    enum class Direction {
        None,
        Download,
        Upload,
        Map,
        Unmap
    };

    void resetRuntimeState() noexcept;
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
};

} // namespace media::ffmpeg::graph
