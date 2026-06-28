#include "internal/graph/runtime/streaming/MediaStreamingSession.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaStreamingSession::prepare(MediaPipelinePresetKind presetKind,
                                                const MediaPipelinePresetOptions& options)
{
    auto graph = MediaPipelinePreset::create(presetKind, options);
    if (!graph) {
        m_state = MediaStreamingSessionState::Failed;
        return ::media::Status::failure(graph.error());
    }

    auto compileStatus = m_runtime.compile(std::move(graph).value());
    if (!compileStatus) {
        m_state = MediaStreamingSessionState::Failed;
        return compileStatus;
    }

    auto registerStatus = m_runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        m_state = MediaStreamingSessionState::Failed;
        return registerStatus;
    }

    m_state = MediaStreamingSessionState::Prepared;
    return ::media::Status::success();
}

::media::Status MediaStreamingSession::start(bool threaded)
{
    auto status = threaded ? m_runtime.startThreaded() : m_runtime.start();
    if (!status) {
        m_state = MediaStreamingSessionState::Failed;
        return status;
    }

    m_state = MediaStreamingSessionState::Running;
    return ::media::Status::success();
}

::media::Status MediaStreamingSession::stop()
{
    auto status = m_runtime.stop();
    if (!status) {
        m_state = MediaStreamingSessionState::Failed;
        return status;
    }

    m_state = MediaStreamingSessionState::Stopped;
    return ::media::Status::success();
}

void MediaStreamingSession::abort() noexcept
{
    m_runtime.abort();
    m_state = MediaStreamingSessionState::Failed;
}

MediaGraphRuntime& MediaStreamingSession::runtime() noexcept
{
    return m_runtime;
}

const MediaGraphRuntime& MediaStreamingSession::runtime() const noexcept
{
    return m_runtime;
}

MediaStreamingSessionState MediaStreamingSession::state() const noexcept
{
    return m_state;
}

MediaGraphRuntimeReport MediaStreamingSession::report() const
{
    return MediaGraphRuntimeReporter::capture(m_runtime);
}

} // namespace media::ffmpeg::graph
