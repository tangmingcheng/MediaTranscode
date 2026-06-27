#include "internal/graph/runtime/MediaGraphRuntime.h"

#include "internal/graph/runtime/MediaGraphLifecycle.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRuntime::compile(MediaGraph graph)
{
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
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

void MediaGraphRuntime::setThreadingPolicy(MediaThreadingPolicy policy) noexcept
{
    m_threadingPolicy = policy;
    m_threadedExecutor.setPolicy(policy);
}

const MediaThreadingPolicy& MediaGraphRuntime::threadingPolicy() const noexcept
{
    return m_threadingPolicy;
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

::media::Status MediaGraphRuntime::startThreaded()
{
    if (!m_context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime startThreaded failed: graph is not compiled"));
    }

    if (m_state == MediaGraphRuntimeState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime startThreaded failed: single-thread runtime is running"));
    }

    m_threadedExecutor.setPolicy(m_threadingPolicy);
    auto status = m_threadedExecutor.start(m_context, m_scheduler);
    if (!status) {
        return status;
    }

    m_state = MediaGraphRuntimeState::ThreadedRunning;
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

    ::media::Status schedulerStatus = ::media::Status::success();
    if (m_state == MediaGraphRuntimeState::ThreadedRunning) {
        schedulerStatus = m_threadedExecutor.stop(m_context, m_scheduler);
    } else {
        schedulerStatus = m_scheduler.stop(m_context);
    }

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
    if (m_state == MediaGraphRuntimeState::ThreadedRunning) {
        m_threadedExecutor.abort(m_context, m_scheduler);
    } else {
        m_scheduler.abort(m_context);
    }
    MediaGraphLifecycle::abortChannels(m_context);
    m_state = MediaGraphRuntimeState::Aborted;
}

void MediaGraphRuntime::reset()
{
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
        abort();
    }

    m_threadedExecutor.clear();
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
    return m_state == MediaGraphRuntimeState::Running ||
           m_state == MediaGraphRuntimeState::ThreadedRunning;
}

bool MediaGraphRuntime::threadedRunning() const noexcept
{
    return m_state == MediaGraphRuntimeState::ThreadedRunning;
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

MediaGraphThreadedExecutor& MediaGraphRuntime::threadedExecutor() noexcept
{
    return m_threadedExecutor;
}

const MediaGraphThreadedExecutor& MediaGraphRuntime::threadedExecutor() const noexcept
{
    return m_threadedExecutor;
}

const MediaGraph* MediaGraphRuntime::graph() const noexcept
{
    return m_context.graph();
}

} // namespace media::ffmpeg::graph
