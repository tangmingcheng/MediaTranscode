#pragma once

#include "internal/graph/nodes/mux/MediaMuxSession.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

#include <optional>
#include <vector>

struct AVFormatContext;

namespace media::ffmpeg::graph {

class FFmpegFileMuxSession final : public MediaMuxSession {
public:
    FFmpegFileMuxSession(bool expectVideo, bool expectAudio) noexcept;
    ~FFmpegFileMuxSession() override;

    ::media::Status bindResource(MediaGraphExecutionContext& context,
                                 const MediaBufferRef& buffer) override;
    ::media::Status bindStreamConfig(MediaGraphExecutionContext& context,
                                     const MediaBufferRef& buffer) override;
    ::media::Status write(MediaGraphExecutionContext& context,
                          const MediaBufferRef& buffer) override;
    ::media::Result<MediaMuxSessionPollResult> poll(
        MediaGraphExecutionContext& context) override;
    bool bindingsReady() const noexcept override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status finish(MediaGraphExecutionContext& context) override;
    void abort() noexcept override;

private:
    ::media::Status validatePlannedStream(MediaStreamKind kind) const;
    ::media::Status registerPendingStreamConfigs();
    ::media::Status registerStreamFromConfig(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecParameters(const MediaBufferRef& buffer);
    ::media::Status writeHeaderIfReady();
    ::media::Status writePendingPacketsIfReady();
    ::media::Status writePacketNow(const MediaBufferRef& buffer);
    ::media::Status writeTrailerIfNeeded();
    ::media::Status preserve(::media::Status status);
    ::media::Status terminalStatus() const;
    bool expectedStreamsRegistered() const noexcept;
    void release() noexcept;

    ::media::ffmpeg::OutputFormatContextPtr m_outputContextOwner;
    AVFormatContext* m_outputContext = nullptr;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_finished = false;
    int m_videoStreamIndex = invalidMediaStreamIndex;
    int m_audioStreamIndex = invalidMediaStreamIndex;
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
    std::vector<MediaBufferRef> m_pendingPackets;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
