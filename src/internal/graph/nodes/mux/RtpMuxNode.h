#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg::graph {

class RtpMuxNode final : public FFmpegNodeRuntime {
public:
    explicit RtpMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status configureExpectations(MediaGraphExecutionContext& context);
    bool tryBindOutputContext(const MediaBufferRef& buffer) noexcept;
    ::media::Status tryBindStreamConfig(const MediaBufferRef& buffer);
    ::media::Status registerPendingStreamConfigs();
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    MediaStreamKind expectedStreamKind() const noexcept;
    const char* expectedStreamName() const noexcept;
    ::media::Status writeHeaderIfNeeded();
    ::media::Status writePendingPacketsIfReady();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writePacketNow(const MediaBufferRef& buffer);
    ::media::Status emitFormatIfReady(MediaGraphExecutionContext& context);
    ::media::Status writeTrailerIfNeeded();
    void releaseRuntimeViews() noexcept;
    bool expectedStreamsRegistered() const noexcept;

private:
    ::media::ffmpeg::OutputFormatContextPtr m_outputContextOwner;
    AVFormatContext* m_outputContext = nullptr;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_formatEmitted = false;
    bool m_expectationsBound = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    int m_streamIndex = invalidMediaStreamIndex;
    std::size_t m_packetsWritten = 0;
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
    std::vector<MediaBufferRef> m_pendingPackets;
};

} // namespace media::ffmpeg::graph
