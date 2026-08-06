#include "internal/graph/runtime/buffer/MediaRawRtpPreparedReplayClock.h"

#include "internal/graph/time/MediaSteadyClock.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Status MediaRawRtpPreparedReplayClock::observe(
    std::int64_t observedAtNs)
{
    if (observedAtNs <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared replay observed an invalid arrival time"));
    }
    std::scoped_lock lock(m_mutex);
    if (m_runtimeOriginNs) {
        if (observedAtNs < *m_sourceOriginNs) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP prepared replay arrival preceded its activated source epoch"));
        }
        return ::media::Status::success();
    }
    if (!m_sourceOriginNs) {
        m_sourceOriginNs = observedAtNs;
    } else {
        m_sourceOriginNs = (std::min)(*m_sourceOriginNs, observedAtNs);
    }
    return ::media::Status::success();
}

::media::Result<MediaRawRtpPreparedReplayEpoch>
MediaRawRtpPreparedReplayClock::activate()
{
    std::scoped_lock lock(m_mutex);
    if (!m_sourceOriginNs) {
        return ::media::Result<MediaRawRtpPreparedReplayEpoch>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared replay has no observed source epoch"));
    }
    if (!m_runtimeOriginNs) m_runtimeOriginNs = mediaSteadyClockNowNs();
    return ::media::Result<MediaRawRtpPreparedReplayEpoch>::success({
        *m_sourceOriginNs, *m_runtimeOriginNs});
}

} // namespace media::ffmpeg::graph
