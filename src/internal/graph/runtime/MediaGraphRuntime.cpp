#include "internal/graph/runtime/MediaGraphRuntime.h"

#include "internal/graph/runtime/MediaGraphLifecycle.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRuntime::compile(MediaGraph graph)
{
    if (m_state == MediaGraphRuntimeState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime compile failed: runtime is running"));
    }

    m_context.reset();
    m_graph = std::move(graph);

    auto status = m_context.compile(m_graph);
    if (!status) {
        m_state = MediaGraphRuntimeState::Empty;
        return status;
    }

    m_state = MediaGraphRuntimeState::Compiled;
    return ::media::Status::success();
}

::media::Status MediaGraphRuntime::registerRuntimeNode(std::unique_ptr<MediaRuntimeNode> node)
{
    return m_scheduler.registerNode(std::move(node));
}

::media::Status MediaGraphRuntime::start()
{
    if (!m_context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime start failed: graph is not compiled"));
    }

    auto status = m_scheduler.start(m_context);
    if (!status) {
        return status;
    }

    m_state = MediaGraphRuntimeState::Running;
    return ::media::Status::success();
}

::media::Status MediaGraphRuntime::processOnce()
{
    if (m_state != MediaGraphRuntimeState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime processOnce failed: runtime is not running"));
    }

    return m_scheduler.processOnce(m_context);
}

::media::Status MediaGraphRuntime::flush()
{
    if (!m_context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime flush failed: graph is not compiled"));
    }

    return m_scheduler.flush(m_context);
}

::media::Status MediaGraphRuntime::stop()
{
    if (!m_context.compiled()) {
        m_state = MediaGraphRuntimeState::Stopped;
        return ::media::Status::success();
    }

    auto schedulerStatus = m_scheduler.stop(m_context);
    auto closeStatus = MediaGraphLifecycle::closeChannels(m_context);

    if (!schedulerStatus) {
        return schedulerStatus;
    }

    if (!closeStatus) {
        return closeStatus;
    }

    m_state = MediaGraphRuntimeState::Stopped;
    return ::media::Status::success();
}

void MediaGraphRuntime::abort() noexcept
{
    m_scheduler.abort(m_context);
    MediaGraphLifecycle::abortChannels(m_context);
    m_state = MediaGraphRuntimeState::Aborted;
}

void MediaGraphRuntime::reset()
{
    if (m_state == MediaGraphRuntimeState::Running) {
        abort();
    }

    m_scheduler.clear();
    m_context.reset();
    m_graph.clear();
    m_state = MediaGraphRuntimeState::Empty;
}

MediaGraphRuntimeState MediaGraphRuntime::state() const noexcept
{
    return m_state;
}

bool MediaGraphRuntime::compiled() const noexcept
{
    return m_context.compiled();
}

bool MediaGraphRuntime::running() const noexcept
{
    return m_state == MediaGraphRuntimeState::Running;
}

MediaGraphExecutionContext& MediaGraphRuntime::context() noexcept
{
    return m_context;
}

const MediaGraphExecutionContext& MediaGraphRuntime::context() const noexcept
{
    return m_context;
}

MediaGraphScheduler& MediaGraphRuntime::scheduler() noexcept
{
    return m_scheduler;
}

const MediaGraphScheduler& MediaGraphRuntime::scheduler() const noexcept
{
    return m_scheduler;
}

const MediaGraph* MediaGraphRuntime::graph() const noexcept
{
    return m_context.graph();
}

} // namespace media::ffmpeg::graph
