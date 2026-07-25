#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"

namespace media::ffmpeg::graph {

class ScheduledRtpMuxFfmpegSessionFactory final
    : public ScheduledRtpPacketizerFactory {
public:
    ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink) override;
};

} // namespace media::ffmpeg::graph
