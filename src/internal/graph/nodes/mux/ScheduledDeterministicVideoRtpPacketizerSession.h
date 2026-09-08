#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"

#include <optional>

namespace media::ffmpeg::graph {

class ScheduledDeterministicVideoRtpPacketizerSession final
    : public ScheduledRtpPacketizerSession {
public:
    ScheduledDeterministicVideoRtpPacketizerSession(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink);

    ::media::Status open() override;
    ::media::Status writeAccessUnit(
        const AVPacket& packet,
        MediaRtpTimestamp timestamp) override;

private:
    ScheduledRtpMuxStreamConfig m_config;
    ScheduledRtpRewrittenDatagramSink m_sink;
    bool m_open = false;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
