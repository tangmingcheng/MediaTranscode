#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.h"


#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

MediaGraphRuntime::MediaGraphRuntime(
    std::shared_ptr<MediaAvSyncClockSource> avSyncClockSource)
    : m_avSyncClockSource(std::move(avSyncClockSource))
{
}

void MediaGraphRuntime::setDiagnosticsEnabled(bool enabled) noexcept
{
    m_context.setDiagnosticsEnabled(enabled);
}

bool MediaGraphRuntime::diagnosticsEnabled() const noexcept
{
    return m_context.diagnosticsEnabled();
}

::media::Status MediaGraphRuntime::compile(MediaGraph graph)
{
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime compile failed: runtime is running"));
    }

    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graph);
    return compileTransaction(std::move(executable));
}

::media::Status MediaGraphRuntime::compile(MediaRealtimeExecutableGraph executable)
{
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime compile failed: runtime is running"));
    }
    return compileTransaction(std::move(executable));
}

::media::Status MediaGraphRuntime::compileTransaction(
    MediaRealtimeExecutableGraph executable)
{
    return MediaGraphRuntimeCompiler::compile(
        std::move(executable), m_graph, m_inputBindings,
        m_playbackEpochActivationCapability,
        m_avSyncClockSource,
        m_context, m_scheduler, m_threadedExecutor, m_acceptanceCollector,
        m_queueHighWatermark, m_state);
}

::media::Status MediaGraphRuntime::registerRuntimeNode(std::unique_ptr<MediaRuntimeNode> node)
{
    return MediaGraphRuntimeCompiler::registerNode(m_scheduler, std::move(node));
}

::media::Status MediaGraphRuntime::registerDefaultRuntimeNodes()
{
    return MediaGraphRuntimeCompiler::registerDefaults(
        m_context, m_scheduler, m_inputBindings,
        m_playbackEpochActivationCapability);
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

::media::Result<MediaGraphRunResult> MediaGraphRuntime::run()
{
    return MediaGraphRuntimeLifecycleExecutor::run(*this);
}

::media::Status MediaGraphRuntime::startThreaded()
{
    return MediaGraphRuntimeLifecycleExecutor::startThreaded(*this);
}

::media::Status MediaGraphRuntime::flush()
{
    return MediaGraphRuntimeLifecycleExecutor::flush(*this);
}

::media::Status MediaGraphRuntime::synchronizeThreadedState()
{
    return MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(*this);
}

::media::Status MediaGraphRuntime::stop()
{
    return MediaGraphRuntimeLifecycleExecutor::stop(*this);
}

void MediaGraphRuntime::abort() noexcept
{
    MediaGraphRuntimeLifecycleExecutor::abort(*this);
}

void MediaGraphRuntime::reset()
{
    MediaGraphRuntimeLifecycleExecutor::reset(*this);
}

MediaGraphRuntimeState MediaGraphRuntime::state() const noexcept
{
    return m_state;
}

bool MediaGraphRuntime::compiled() const noexcept
{
    return m_state == MediaGraphRuntimeState::Compiled ||
           m_state == MediaGraphRuntimeState::Running ||
           m_state == MediaGraphRuntimeState::ThreadedRunning ||
           m_state == MediaGraphRuntimeState::Stopped;
}

bool MediaGraphRuntime::running() const noexcept
{
    return m_state == MediaGraphRuntimeState::Running;
}

bool MediaGraphRuntime::threadedRunning() const noexcept
{
    return state() == MediaGraphRuntimeState::ThreadedRunning;
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

MediaRuntimeAcceptanceCollector& MediaGraphRuntime::acceptanceCollector() noexcept
{
    return m_acceptanceCollector;
}

const MediaRuntimeAcceptanceCollector& MediaGraphRuntime::acceptanceCollector() const noexcept
{
    return m_acceptanceCollector;
}

std::size_t MediaGraphRuntime::observeQueueHighWatermark(std::size_t queued) const noexcept
{
    std::size_t peak = m_queueHighWatermark.load(std::memory_order_relaxed);
    while (queued > peak && !m_queueHighWatermark.compare_exchange_weak(
               peak, queued, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
    return queued > peak ? queued : peak;
}

} // namespace media::ffmpeg::graph
