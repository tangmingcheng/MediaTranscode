#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"

namespace media::ffmpeg::graph {

void RtpMuxStateMachine::reset() noexcept
{
    m_completion.reset();
    m_headerWritten = false;
    m_trailerWritten = false;
    m_formatEmitted = false;
    m_expectationsBound = false;
    m_expectVideo = false;
    m_expectAudio = false;
    m_monotonicPacketTimestamps = false;
    m_startupDelayElapsed = false;
    m_pacingSessionStarted = false;
    m_startupDelayMs = 0;
}

MediaMuxCompletionState& RtpMuxStateMachine::completion() noexcept { return m_completion; }
bool& RtpMuxStateMachine::headerWritten() noexcept { return m_headerWritten; }
bool& RtpMuxStateMachine::trailerWritten() noexcept { return m_trailerWritten; }
bool& RtpMuxStateMachine::formatEmitted() noexcept { return m_formatEmitted; }
bool& RtpMuxStateMachine::expectationsBound() noexcept { return m_expectationsBound; }
bool& RtpMuxStateMachine::expectVideo() noexcept { return m_expectVideo; }
bool RtpMuxStateMachine::expectVideo() const noexcept { return m_expectVideo; }
bool& RtpMuxStateMachine::expectAudio() noexcept { return m_expectAudio; }
bool RtpMuxStateMachine::expectAudio() const noexcept { return m_expectAudio; }
bool& RtpMuxStateMachine::monotonicPacketTimestamps() noexcept { return m_monotonicPacketTimestamps; }
bool& RtpMuxStateMachine::startupDelayElapsed() noexcept { return m_startupDelayElapsed; }
bool& RtpMuxStateMachine::pacingSessionStarted() noexcept { return m_pacingSessionStarted; }
int& RtpMuxStateMachine::startupDelayMs() noexcept { return m_startupDelayMs; }

} // namespace media::ffmpeg::graph
