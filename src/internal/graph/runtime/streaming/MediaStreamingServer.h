#pragma once

#include "internal/graph/runtime/streaming/MediaStreamingSession.h"
#include "media_transcode/Result.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaStreamingServerSessionInfo {
    std::string sessionId;
    MediaStreamingSessionState state = MediaStreamingSessionState::Idle;
    MediaGraphRuntimeReport report;
};

class MediaStreamingServer final {
public:
    ::media::Result<MediaStreamingSession*> createSession(std::string sessionId,
                                                           MediaPipelinePresetKind presetKind,
                                                           const MediaPipelinePresetOptions& options);

    MediaStreamingSession* findSession(const std::string& sessionId);
    const MediaStreamingSession* findSession(const std::string& sessionId) const;

    ::media::Status startSession(const std::string& sessionId, bool threaded = false);
    ::media::Status stopSession(const std::string& sessionId);
    bool removeSession(const std::string& sessionId);
    void abortAll() noexcept;

    std::vector<MediaStreamingServerSessionInfo> sessions() const;
    std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, std::unique_ptr<MediaStreamingSession>> m_sessions;
};

} // namespace media::ffmpeg::graph
