#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class AvPacketStartBarrierNode final : public FFmpegNodeRuntime {
public:
    explicit AvPacketStartBarrierNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status processCodec(MediaGraphExecutionContext& context,
                                 const char* inputPort,
                                 const char* outputPort);
    ::media::Status processPacket(MediaGraphExecutionContext& context,
                                  const char* inputPort,
                                  const char* outputPort,
                                  bool expected,
                                  bool& ready,
                                  MediaBufferRef& pending);
    ::media::Status releaseIfReady(MediaGraphExecutionContext& context);
    void reset() noexcept;

private:
    bool m_configured = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    bool m_requireVideoKeyFrame = false;
    bool m_open = false;
    bool m_videoReady = false;
    bool m_audioReady = false;
    MediaBufferRef m_pendingVideo;
    MediaBufferRef m_pendingAudio;
};

} // namespace media::ffmpeg::graph
