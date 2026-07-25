#pragma once

#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"
#include <media_transcode/Result.h>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph {

class RtpMuxStateMachine final {
public:
    void reset() noexcept;
    ::media::Status bindExpectations(bool expectVideo, bool expectAudio,
                                     bool monotonicPacketTimestamps, int startupDelayMs);
    ::media::Status bindOutput();
    void cancelOutputBinding() noexcept;
    ::media::Status markHeaderWritten();
    ::media::Status markFormatEmitted();
    ::media::Status markStartupDelayElapsed();
    ::media::Status markPacingSessionStarted();
    ::media::Status markTrailerWritten();
    void setExpectedInputs(std::vector<std::string> configKeys,
                           std::vector<std::string> terminalChannels);
    bool markConfigReady(std::string input);
    bool markInputEof(std::string_view input);
    bool markInputClosed(std::string_view input);
    void setPendingPackets(std::size_t count) noexcept;
    bool readyForTrailer() const noexcept;
    bool finished() const noexcept;

    bool outputBound() const noexcept;
    bool headerWritten() const noexcept;
    bool trailerWritten() const noexcept;
    bool formatEmitted() const noexcept;
    bool expectationsBound() const noexcept;
    bool expectVideo() const noexcept;
    bool expectAudio() const noexcept;
    bool monotonicPacketTimestamps() const noexcept;
    bool startupDelayElapsed() const noexcept;
    bool pacingSessionStarted() const noexcept;
    int startupDelayMs() const noexcept;

private:
    MediaMuxCompletionState m_completion;
    bool m_headerWritten = false;
    bool m_outputBound = false;
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
