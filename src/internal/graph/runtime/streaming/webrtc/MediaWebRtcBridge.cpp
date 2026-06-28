#include "internal/graph/runtime/streaming/webrtc/MediaWebRtcBridge.h"

namespace media::ffmpeg::graph {

::media::Status MediaWebRtcBridge::attach(MediaStreamingSession* session)
{
    if (!session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaWebRtcBridge attach failed: session is null"));
    }

    m_session = session;
    return ::media::Status::success();
}

void MediaWebRtcBridge::detach() noexcept
{
    m_session = nullptr;
    m_peerCount = 0;
}

::media::Status MediaWebRtcBridge::addPeer(const MediaWebRtcPeerConfig& config)
{
    if (!m_session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaWebRtcBridge addPeer failed: bridge is not attached"));
    }

    if (config.peerId.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaWebRtcBridge addPeer failed: peerId is empty"));
    }

    ++m_peerCount;
    return ::media::Status::success();
}

::media::Status MediaWebRtcBridge::removePeer(const std::string& peerId)
{
    if (peerId.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaWebRtcBridge removePeer failed: peerId is empty"));
    }

    if (m_peerCount > 0) {
        --m_peerCount;
    }

    return ::media::Status::success();
}

std::size_t MediaWebRtcBridge::peerCount() const noexcept
{
    return m_peerCount;
}

bool MediaWebRtcBridge::attached() const noexcept
{
    return m_session != nullptr;
}

} // namespace media::ffmpeg::graph
