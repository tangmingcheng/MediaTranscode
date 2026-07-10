#include "internal/graph/nodes/mux/RtpMuxFfmpegSession.h"

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"

namespace media::ffmpeg::graph {

bool RtpMuxFfmpegSession::bindOutput(const MediaBufferRef& buffer) noexcept
{
    auto* format = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!format || !format->context()) return false;
    if (format->ownership() == FFmpegFormatContextOwnership::Output) {
        m_owner = format->takeOutputContext();
        m_context = m_owner.get();
    } else {
        m_owner.reset();
        m_context = format->context();
    }
    m_streamIndex = invalidMediaStreamIndex;
    m_nextPacketDts = AV_NOPTS_VALUE;
    m_packetsWritten = 0;
    m_pacingClock.reset();
    return m_context != nullptr;
}

void RtpMuxFfmpegSession::reset() noexcept
{
    m_context = nullptr;
    m_owner.reset();
    m_streamIndex = invalidMediaStreamIndex;
    m_nextPacketDts = AV_NOPTS_VALUE;
    m_packetsWritten = 0;
    m_pacingClock.reset();
}

AVFormatContext* RtpMuxFfmpegSession::context() const noexcept { return m_context; }
int& RtpMuxFfmpegSession::streamIndex() noexcept { return m_streamIndex; }
int RtpMuxFfmpegSession::streamIndex() const noexcept { return m_streamIndex; }
int64_t& RtpMuxFfmpegSession::nextPacketDts() noexcept { return m_nextPacketDts; }
std::size_t& RtpMuxFfmpegSession::packetsWritten() noexcept { return m_packetsWritten; }
std::size_t RtpMuxFfmpegSession::packetsWritten() const noexcept { return m_packetsWritten; }
MediaGraphPacingClock& RtpMuxFfmpegSession::pacingClock() noexcept { return m_pacingClock; }

} // namespace media::ffmpeg::graph
