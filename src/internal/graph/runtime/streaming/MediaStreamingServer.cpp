#include "internal/graph/runtime/streaming/MediaStreamingServer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaStreamingSession*> MediaStreamingServer::createSession(
    std::string sessionId,
    MediaPipelinePresetKind presetKind,
    const MediaPipelinePresetOptions& options)
{
    if (sessionId.empty()) {
        return ::media::Result<MediaStreamingSession*>::failure(
            ::media::ErrorInfo::invalidArgument("MediaStreamingServer createSession failed: sessionId is empty"));
    }

    if (m_sessions.find(sessionId) != m_sessions.end()) {
        return ::media::Result<MediaStreamingSession*>::failure(
            ::media::ErrorInfo::invalidArgument("MediaStreamingServer createSession failed: duplicate sessionId"));
    }

    auto session = std::make_unique<MediaStreamingSession>();
    auto status = session->prepare(presetKind, options);
    if (!status) {
        return ::media::Result<MediaStreamingSession*>::failure(status.error());
    }

    MediaStreamingSession* raw = session.get();
    m_sessions.emplace(std::move(sessionId), std::move(session));
    return ::media::Result<MediaStreamingSession*>::success(raw);
}

MediaStreamingSession* MediaStreamingServer::findSession(const std::string& sessionId)
{
    const auto it = m_sessions.find(sessionId);
    return it == m_sessions.end() ? nullptr : it->second.get();
}

const MediaStreamingSession* MediaStreamingServer::findSession(const std::string& sessionId) const
{
    const auto it = m_sessions.find(sessionId);
    return it == m_sessions.end() ? nullptr : it->second.get();
}

::media::Status MediaStreamingServer::startSession(const std::string& sessionId, bool threaded)
{
    MediaStreamingSession* session = findSession(sessionId);
    if (!session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaStreamingServer startSession failed: session not found"));
    }

    return session->start(threaded);
}

::media::Status MediaStreamingServer::stopSession(const std::string& sessionId)
{
    MediaStreamingSession* session = findSession(sessionId);
    if (!session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaStreamingServer stopSession failed: session not found"));
    }

    return session->stop();
}

bool MediaStreamingServer::removeSession(const std::string& sessionId)
{
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        return false;
    }

    it->second->abort();
    m_sessions.erase(it);
    return true;
}

void MediaStreamingServer::abortAll() noexcept
{
    for (auto& item : m_sessions) {
        if (item.second) {
            item.second->abort();
        }
    }
}

std::vector<MediaStreamingServerSessionInfo> MediaStreamingServer::sessions() const
{
    std::vector<MediaStreamingServerSessionInfo> result;
    result.reserve(m_sessions.size());

    for (const auto& item : m_sessions) {
        MediaStreamingServerSessionInfo info;
        info.sessionId = item.first;
        if (item.second) {
            info.state = item.second->state();
            info.report = item.second->report();
        }
        result.push_back(std::move(info));
    }

    return result;
}

std::size_t MediaStreamingServer::size() const noexcept
{
    return m_sessions.size();
}

} // namespace media::ffmpeg::graph
