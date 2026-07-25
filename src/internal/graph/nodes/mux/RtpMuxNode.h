#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/nodes/mux/RtpMuxFfmpegSession.h"
#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"

#include <vector>
#include <chrono>

namespace media::ffmpeg::graph {

class RtpMuxNode final : public FFmpegNodeRuntime {
public:
    explicit RtpMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status configureExpectations(MediaGraphExecutionContext& context);
    bool tryBindOutputContext(const MediaBufferRef& buffer) noexcept;
    ::media::Status tryBindStreamConfig(const MediaBufferRef& buffer);
    ::media::Status registerPendingStreamConfigs();
    ::media::Status registerStreamFromConfig(const MediaBufferRef& buffer);
    MediaStreamKind expectedStreamKind() const noexcept;
    const char* expectedStreamName() const noexcept;
    ::media::Status writeHeaderIfNeeded();
    ::media::Status startPacingSessionIfNeeded();
    ::media::Status writePendingPacketsIfReady();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writePacketNow(const MediaBufferRef& buffer);
    ::media::Status emitFormatIfReady(MediaGraphExecutionContext& context);
    ::media::Status writeTrailerIfNeeded();
    void releaseRuntimeViews() noexcept;
    bool expectedStreamsRegistered() const noexcept;

private:
    RtpMuxFfmpegSession m_session;
    RtpMuxStateMachine m_state;
    std::chrono::steady_clock::time_point m_startupReadyAt {};
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
    std::vector<MediaBufferRef> m_pendingPackets;
};

} // namespace media::ffmpeg::graph
