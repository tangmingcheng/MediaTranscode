#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSessionFactory.h"

#include "internal/graph/nodes/mux/ScheduledDeterministicVideoRtpPacketizerSession.h"
#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>
ScheduledRtpPacketizerSessionFactory::create(
    ScheduledRtpMuxStreamConfig config,
    ScheduledRtpRewrittenDatagramSink sink)
{
    using Result =
        ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>;
    if (config.streamKind() == MediaStreamKind::Video) {
        auto session =
            std::unique_ptr<ScheduledDeterministicVideoRtpPacketizerSession>(
                new (std::nothrow)
                    ScheduledDeterministicVideoRtpPacketizerSession(
                        std::move(config), std::move(sink)));
        return session
            ? Result::success(std::move(session))
            : Result::failure(::media::ErrorInfo::allocationFailed(
                  "ScheduledDeterministicVideoRtpPacketizerSession"));
    }
    auto session = std::unique_ptr<ScheduledRtpMuxFfmpegSession>(
        new (std::nothrow) ScheduledRtpMuxFfmpegSession(std::move(sink)));
    if (!session) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "ScheduledRtpMuxFfmpegSession"));
    }
    auto configured = session->configure(std::move(config));
    return configured ? Result::success(std::move(session))
                      : Result::failure(configured.error());
}

} // namespace media::ffmpeg::graph
