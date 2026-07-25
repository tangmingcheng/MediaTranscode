#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>
ScheduledRtpMuxFfmpegSessionFactory::create(
    ScheduledRtpMuxStreamConfig config,
    ScheduledRtpRewrittenDatagramSink sink)
{
    using SessionResult =
        ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>;
    auto session = std::unique_ptr<ScheduledRtpMuxFfmpegSession>(
        new (std::nothrow) ScheduledRtpMuxFfmpegSession(std::move(sink)));
    if (!session) {
        return SessionResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "ScheduledRtpMuxFfmpegSession"));
    }
    auto configured = session->configure(std::move(config));
    if (!configured) return SessionResult::failure(configured.error());
    return SessionResult::success(std::move(session));
}

} // namespace media::ffmpeg::graph
