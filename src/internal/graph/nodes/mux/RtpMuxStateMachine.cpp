#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"

#include <utility>

namespace media::ffmpeg::graph {

namespace {
::media::Status invalidTransition(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}
}

void RtpMuxStateMachine::reset() noexcept
{
    m_completion.reset();
    m_headerWritten = false;
    m_outputBound = false;
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

::media::Status RtpMuxStateMachine::bindExpectations(bool video, bool audio, bool monotonic, int delayMs)
{
    if (m_expectationsBound || video == audio || delayMs < 0) return invalidTransition("invalid RTP mux expectation transition");
    m_expectVideo = video; m_expectAudio = audio; m_monotonicPacketTimestamps = monotonic;
    m_startupDelayMs = delayMs; m_startupDelayElapsed = delayMs == 0; m_expectationsBound = true;
    return ::media::Status::success();
}
::media::Status RtpMuxStateMachine::bindOutput()
{
    if (!m_expectationsBound || m_outputBound || m_headerWritten) return invalidTransition("RTP mux output cannot be bound in current state");
    m_outputBound = true;
    m_headerWritten = false; m_trailerWritten = false; m_formatEmitted = false;
    return ::media::Status::success();
}
void RtpMuxStateMachine::cancelOutputBinding() noexcept
{
    if (!m_headerWritten) m_outputBound = false;
}
::media::Status RtpMuxStateMachine::markHeaderWritten() { if (!m_expectationsBound || !m_outputBound || m_headerWritten || m_trailerWritten) return invalidTransition("invalid RTP mux header transition"); m_headerWritten=true; m_completion.markHeaderWritten(); return ::media::Status::success(); }
::media::Status RtpMuxStateMachine::markFormatEmitted() { if (!m_headerWritten || m_formatEmitted || m_trailerWritten) return invalidTransition("invalid RTP mux format transition"); m_formatEmitted=true; return ::media::Status::success(); }
::media::Status RtpMuxStateMachine::markStartupDelayElapsed() { if (!m_expectationsBound || m_startupDelayElapsed) return invalidTransition("invalid RTP mux startup transition"); m_startupDelayElapsed=true; return ::media::Status::success(); }
::media::Status RtpMuxStateMachine::markPacingSessionStarted() { if (!m_startupDelayElapsed || m_pacingSessionStarted || m_trailerWritten) return invalidTransition("invalid RTP mux pacing transition"); m_pacingSessionStarted=true; return ::media::Status::success(); }
::media::Status RtpMuxStateMachine::markTrailerWritten() { if (!m_headerWritten || m_trailerWritten || !m_completion.readyForTrailer()) return invalidTransition("invalid RTP mux trailer transition"); m_trailerWritten=true; m_completion.markTrailerWritten(); return ::media::Status::success(); }
void RtpMuxStateMachine::setExpectedInputs(std::vector<std::string> configKeys,
                                          std::vector<std::string> terminalChannels)
{
    m_completion.setExpectedConfigKeys(std::move(configKeys));
    m_completion.setExpectedTerminalChannels(std::move(terminalChannels));
}
bool RtpMuxStateMachine::markConfigReady(std::string input) { return m_completion.markConfigReady(std::move(input)); }
bool RtpMuxStateMachine::markInputEof(std::string_view input) { return m_completion.markInputEof(input); }
bool RtpMuxStateMachine::markInputClosed(std::string_view input) { return m_completion.markInputClosed(input); }
void RtpMuxStateMachine::setPendingPackets(std::size_t count) noexcept { m_completion.setPendingPackets(count); }
bool RtpMuxStateMachine::readyForTrailer() const noexcept { return m_completion.readyForTrailer(); }
bool RtpMuxStateMachine::finished() const noexcept { return m_completion.finished(); }
bool RtpMuxStateMachine::outputBound() const noexcept { return m_outputBound; }
bool RtpMuxStateMachine::headerWritten() const noexcept { return m_headerWritten; }
bool RtpMuxStateMachine::trailerWritten() const noexcept { return m_trailerWritten; }
bool RtpMuxStateMachine::formatEmitted() const noexcept { return m_formatEmitted; }
bool RtpMuxStateMachine::expectationsBound() const noexcept { return m_expectationsBound; }
bool RtpMuxStateMachine::expectVideo() const noexcept { return m_expectVideo; }
bool RtpMuxStateMachine::expectAudio() const noexcept { return m_expectAudio; }
bool RtpMuxStateMachine::monotonicPacketTimestamps() const noexcept { return m_monotonicPacketTimestamps; }
bool RtpMuxStateMachine::startupDelayElapsed() const noexcept { return m_startupDelayElapsed; }
bool RtpMuxStateMachine::pacingSessionStarted() const noexcept { return m_pacingSessionStarted; }
int RtpMuxStateMachine::startupDelayMs() const noexcept { return m_startupDelayMs; }

} // namespace media::ffmpeg::graph
