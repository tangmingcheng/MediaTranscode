#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/mux/MediaMuxSessionFactory.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class FileMuxNode final : public FFmpegNodeRuntime {
public:
    explicit FileMuxNode(MediaNodeId nodeId);
    FileMuxNode(MediaNodeId nodeId,
                std::unique_ptr<MediaMuxSessionFactory> sessionFactory);
    static MediaNodeKind staticKind() noexcept;

    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status ensureSession(MediaGraphExecutionContext& context);
    ::media::Status bindCompletionInputs(MediaGraphExecutionContext& context);
    void observeClosedInputs(MediaGraphExecutionContext& context);
    ::media::Status handleBuffer(MediaGraphExecutionContext& context,
                                 const PoppedChannelBuffer& input);
    ::media::Result<MediaNodeProcessResult> finishIfReady(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& terminalBuffer = {});
    ::media::Result<MediaNodeProcessResult> pollOrWait(
        MediaGraphExecutionContext& context);
    ::media::Status remember(::media::Status status);
    ::media::Result<MediaNodeProcessResult> terminalResult() const;
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context,
                                          const MediaBufferRef& buffer);
    void releaseSession() noexcept;

    std::unique_ptr<MediaMuxSessionFactory> m_sessionFactory;
    std::unique_ptr<MediaMuxSession> m_session;
    std::unique_ptr<MediaInputTerminalTracker> m_completion;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    bool m_abortForwarded = false;
};

} // namespace media::ffmpeg::graph
