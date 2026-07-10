#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/MediaGraphPacingClock.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"

#include <vector>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
}

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
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecParameters(const MediaBufferRef& buffer);
    MediaStreamKind expectedStreamKind() const noexcept;
    const char* expectedStreamName() const noexcept;
    ::media::Status writeHeaderIfNeeded();
    ::media::Status startPacingSessionIfNeeded();
    ::media::Status writePendingPacketsIfReady();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writePacketNow(const MediaBufferRef& buffer);
    ::media::Status normalizePacketTimestamps(AVPacket& packet);
    ::media::Status emitFormatIfReady(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> makeSdpFormatSnapshot() const;
    ::media::Status writeTrailerIfNeeded();
    void releaseRuntimeViews() noexcept;
    bool expectedStreamsRegistered() const noexcept;

private:
    ::media::ffmpeg::OutputFormatContextPtr m_outputContextOwner;
    AVFormatContext* m_outputContext = nullptr;
    MediaGraphPacingClock m_pacingClock;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_formatEmitted = false;
    bool m_expectationsBound = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    bool m_monotonicPacketTimestamps = false;
    bool m_startupDelayElapsed = false;
    bool m_pacingSessionStarted = false;
    int m_streamIndex = invalidMediaStreamIndex;
    int m_startupDelayMs = 0;
    int64_t m_nextPacketDts = AV_NOPTS_VALUE;
    std::size_t m_packetsWritten = 0;
    std::chrono::steady_clock::time_point m_startupReadyAt {};
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
    std::vector<MediaBufferRef> m_pendingPackets;
    MediaMuxCompletionState m_completion;
};

} // namespace media::ffmpeg::graph
