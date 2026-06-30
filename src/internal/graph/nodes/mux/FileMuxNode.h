#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg::graph {

class FileMuxNode final : public FFmpegNodeRuntime {
public:
    explicit FileMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindMuxExpectations(MediaGraphExecutionContext& context);
    bool tryBindOutputContext(const MediaBufferRef& buffer) noexcept;
    ::media::Status tryBindStreamConfig(const MediaBufferRef& buffer);
    ::media::Status registerPendingStreamConfigs();
    ::media::Status registerStreamFromConfig(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecParameters(const MediaBufferRef& buffer);
    ::media::Status writeHeaderIfNeeded();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writeTrailerIfNeeded();
    void releaseRuntimeViews() noexcept;
    bool expectedStreamsRegistered() const noexcept;
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    ::media::ffmpeg::OutputFormatContextPtr m_outputContextOwner;
    AVFormatContext* m_outputContext = nullptr;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_expectationsBound = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    int m_videoStreamIndex = invalidMediaStreamIndex;
    int m_audioStreamIndex = invalidMediaStreamIndex;
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
};

} // namespace media::ffmpeg::graph
