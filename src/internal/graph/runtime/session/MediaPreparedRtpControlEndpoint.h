#pragma once
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

namespace media::ffmpeg::graph {
// One frozen stream's control consumer. It never owns transport or purge.
class MediaPreparedRtpControlEndpoint final {
public:
    static ::media::Result<MediaPreparedRtpControlEndpoint> create(
        std::shared_ptr<MediaPreparedRtpSourceSession> source, MediaStreamKind stream);
    MediaPreparedRtpControlEndpoint(MediaPreparedRtpControlEndpoint&&) noexcept = default;
    MediaPreparedRtpControlEndpoint& operator=(MediaPreparedRtpControlEndpoint&&) = delete;
    MediaPreparedRtpControlEndpoint(const MediaPreparedRtpControlEndpoint&) = delete;
    ~MediaPreparedRtpControlEndpoint();
    ::media::Status activate(std::shared_ptr<MediaNodeWakeup> wakeup);
    ::media::Result<std::optional<MediaPreparedRtpOutput>> take();
    void deactivate() noexcept;
private:
    MediaPreparedRtpControlEndpoint(std::shared_ptr<MediaPreparedRtpSourceSession> source,
        MediaStreamKind stream) noexcept : m_source(std::move(source)), m_stream(stream) {}
    std::shared_ptr<MediaPreparedRtpSourceSession> m_source;
    MediaStreamKind m_stream;
    std::shared_ptr<MediaNodeWakeup> m_wakeup;
};
} // namespace media::ffmpeg::graph
