#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <vector>

struct AVFormatContext;

namespace media::ffmpeg::graph {

class FFmpegMuxWriter final {
public:
    ::media::Status configureExpectations(bool expectVideo, bool expectAudio) noexcept;
    bool bindOutputContext(const MediaBufferRef& buffer) noexcept;
    ::media::Status tryBindStreamConfig(const MediaBufferRef& buffer);
    ::media::Status registerPendingStreamConfigs();
    ::media::Status writeHeaderIfNeeded();
    ::media::Status writePendingPacketsIfReady();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writeTrailerIfNeeded();
    void reset() noexcept;

    AVFormatContext* context() noexcept;
    bool headerWritten() const noexcept;
    bool readyForSdp() const noexcept;

private:
    ::media::Status registerStreamFromConfig(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    ::media::Status registerStreamFromCodecParameters(const MediaBufferRef& buffer);
    ::media::Status writePacketNow(const MediaBufferRef& buffer);
    bool expectedStreamsRegistered() const noexcept;

private:
    ::media::ffmpeg::OutputFormatContextPtr m_outputContextOwner;
    MediaBufferRef m_outputContextBuffer;
    AVFormatContext* m_outputContext = nullptr;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_expectationsBound = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    int m_videoStreamIndex = invalidMediaStreamIndex;
    int m_audioStreamIndex = invalidMediaStreamIndex;
    std::vector<MediaBufferRef> m_pendingStreamConfigs;
    std::vector<MediaBufferRef> m_pendingPackets;
};

} // namespace media::ffmpeg::graph
