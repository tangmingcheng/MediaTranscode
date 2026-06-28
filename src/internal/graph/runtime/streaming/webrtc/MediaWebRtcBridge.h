#pragma once

#include "internal/graph/runtime/streaming/MediaStreamingSession.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaWebRtcPeerConfig {
    std::string peerId;
    std::string signalingUrl;
    bool audioEnabled = true;
    bool videoEnabled = true;
};

class MediaWebRtcBridge final {
public:
    ::media::Status attach(MediaStreamingSession* session);
    void detach() noexcept;

    ::media::Status addPeer(const MediaWebRtcPeerConfig& config);
    ::media::Status removePeer(const std::string& peerId);

    std::size_t peerCount() const noexcept;
    bool attached() const noexcept;

private:
    MediaStreamingSession* m_session = nullptr;
    std::size_t m_peerCount = 0;
};

} // namespace media::ffmpeg::graph
