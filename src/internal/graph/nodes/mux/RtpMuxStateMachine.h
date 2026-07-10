#pragma once

#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"

namespace media::ffmpeg::graph {

class RtpMuxStateMachine final {
public:
    void reset() noexcept;

    MediaMuxCompletionState& completion() noexcept;
    bool& headerWritten() noexcept;
    bool& trailerWritten() noexcept;
    bool& formatEmitted() noexcept;
    bool& expectationsBound() noexcept;
    bool& expectVideo() noexcept;
    bool expectVideo() const noexcept;
    bool& expectAudio() noexcept;
    bool expectAudio() const noexcept;
    bool& monotonicPacketTimestamps() noexcept;
    bool& startupDelayElapsed() noexcept;
    bool& pacingSessionStarted() noexcept;
    int& startupDelayMs() noexcept;

private:
    MediaMuxCompletionState m_completion;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    bool m_formatEmitted = false;
    bool m_expectationsBound = false;
    bool m_expectVideo = false;
    bool m_expectAudio = false;
    bool m_monotonicPacketTimestamps = false;
    bool m_startupDelayElapsed = false;
    bool m_pacingSessionStarted = false;
    int m_startupDelayMs = 0;
};

} // namespace media::ffmpeg::graph
