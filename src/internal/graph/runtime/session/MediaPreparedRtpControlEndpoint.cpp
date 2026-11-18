#include "internal/graph/runtime/session/MediaPreparedRtpControlEndpoint.h"
namespace media::ffmpeg::graph {
::media::Result<MediaPreparedRtpControlEndpoint> MediaPreparedRtpControlEndpoint::create(
    std::shared_ptr<MediaPreparedRtpSourceSession> source, MediaStreamKind stream)
{
    using Result = ::media::Result<MediaPreparedRtpControlEndpoint>;
    if (!source || !source->isFrozen() ||
        (stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio) ||
        (stream == MediaStreamKind::Audio && !source->plan().audio))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP control endpoint requires its exact frozen source stream"));
    return Result::success(MediaPreparedRtpControlEndpoint(std::move(source), stream));
}
MediaPreparedRtpControlEndpoint::~MediaPreparedRtpControlEndpoint() { deactivate(); }
::media::Status MediaPreparedRtpControlEndpoint::activate(std::shared_ptr<MediaNodeWakeup> wakeup)
{
    if (!m_source || m_wakeup || !wakeup) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RTP control endpoint cannot activate twice"));
    if (auto status = m_source->activateControl(m_stream, wakeup); !status) return status;
    m_wakeup = std::move(wakeup);
    return ::media::Status::success();
}
::media::Result<std::optional<MediaPreparedRtpOutput>> MediaPreparedRtpControlEndpoint::take()
{
    if (!m_source || !m_wakeup) return ::media::Result<std::optional<MediaPreparedRtpOutput>>::failure(
        ::media::ErrorInfo::notInitialized("RTP control endpoint is inactive"));
    return m_source->takeOutput(m_stream, MediaPreparedRtpOutputClass::Control);
}
void MediaPreparedRtpControlEndpoint::deactivate() noexcept
{
    if (m_source && m_wakeup) m_source->deactivateControl(m_stream, m_wakeup);
    m_wakeup.reset();
}
} // namespace media::ffmpeg::graph
