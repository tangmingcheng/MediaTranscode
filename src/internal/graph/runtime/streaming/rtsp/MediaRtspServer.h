#pragma once

#include "internal/graph/runtime/streaming/MediaStreamingServer.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRtspServerConfig {
    std::string listenAddress = "0.0.0.0";
    int port = 8554;
    std::string mountPrefix = "/live";
};

class MediaRtspServer final {
public:
    void setConfig(MediaRtspServerConfig config);
    const MediaRtspServerConfig& config() const noexcept;

    ::media::Status start();
    ::media::Status stop();
    bool running() const noexcept;

    MediaStreamingServer& sessions() noexcept;
    const MediaStreamingServer& sessions() const noexcept;

private:
    MediaRtspServerConfig m_config;
    MediaStreamingServer m_sessions;
    bool m_running = false;
};

} // namespace media::ffmpeg::graph
