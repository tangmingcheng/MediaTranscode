#pragma once

#include "internal/graph/runtime/MediaGraphPacingClock.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

#include <cstddef>
#include <cstdint>

struct AVFormatContext;

namespace media::ffmpeg::graph {

class RtpMuxFfmpegSession final {
public:
    bool bindOutput(const MediaBufferRef& buffer) noexcept;
    void reset() noexcept;

    AVFormatContext* context() const noexcept;
    int& streamIndex() noexcept;
    int streamIndex() const noexcept;
    int64_t& nextPacketDts() noexcept;
    std::size_t& packetsWritten() noexcept;
    std::size_t packetsWritten() const noexcept;
    MediaGraphPacingClock& pacingClock() noexcept;

private:
    ::media::ffmpeg::OutputFormatContextPtr m_owner;
    AVFormatContext* m_context = nullptr;
    int m_streamIndex = invalidMediaStreamIndex;
    int64_t m_nextPacketDts = AV_NOPTS_VALUE;
    std::size_t m_packetsWritten = 0;
    MediaGraphPacingClock m_pacingClock;
};

} // namespace media::ffmpeg::graph
