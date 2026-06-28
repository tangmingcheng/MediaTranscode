#include "internal/graph/runtime/streaming/rtsp/MediaRtspServer.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaRtspServer::setConfig(MediaRtspServerConfig config)
{
    m_config = std::move(config);
}

const MediaRtspServerConfig& MediaRtspServer::config() const noexcept
{
    return m_config;
}

::media::Status MediaRtspServer::start()
{
    if (m_running) {
        return ::media::Status::success();
    }

    if (m_config.port <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtspServer start failed: invalid port"));
    }

    m_running = true;
    return ::media::Status::success();
}

::media::Status MediaRtspServer::stop()
{
    if (!m_running) {
        return ::media::Status::success();
    }

    m_sessions.abortAll();
    m_running = false;
    return ::media::Status::success();
}

bool MediaRtspServer::running() const noexcept
{
    return m_running;
}

MediaStreamingServer& MediaRtspServer::sessions() noexcept
{
    return m_sessions;
}

const MediaStreamingServer& MediaRtspServer::sessions() const noexcept
{
    return m_sessions;
}

} // namespace media::ffmpeg::graph
