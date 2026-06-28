#include "internal/graph/runtime/MediaGraphRuntime.h"

#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include <utility>

namespace media::ffmpeg::graph {

namespace {

struct ChannelActivitySnapshot {
    std::uint64_t pushed = 0;
    std::uint64_t popped = 0;
    std::uint64_t closed = 0;
    std::uint64_t aborted = 0;
    std::uint64_t cleared = 0;
    std::size_t queued = 0;
};

ChannelActivitySnapshot captureChannelActivity(const MediaGraphExecutionContext& context)
{
    ChannelActivitySnapshot snapshot;
    for (const MediaChannel* channel : context.channels().channels()) {
        if (!channel) {
            continue;
        }

        const auto& metrics = channel->metrics();
        snapshot.pushed += metrics.pushed;
        snapshot.popped += metrics.popped;
        snapshot.closed += metrics.closed;
        snapshot.aborted += metrics.aborted;
        snapshot.cleared += metrics.cleared;
        snapshot.queued += channel->size();
    }
    return snapshot;
}

bool sameChannelActivity(const ChannelActivitySnapshot& lhs,
                         const ChannelActivitySnapshot& rhs) noexcept
{
    return lhs.pushed == rhs.pushed &&
           lhs.popped == rhs.popped &&
           lhs.closed == rhs.closed &&
           lhs.aborted == rhs.aborted &&
           lhs.cleared == rhs.cleared;
}

void copySnapshotToResult(const ChannelActivitySnapshot& snapshot,
                          MediaGraphRunLoopResult& result) noexcept
{
    result.totalPushed = snapshot.pushed;
    result.totalPopped = snapshot.popped;
    result.totalClosed = snapshot.closed;
    result.totalAborted = snapshot.aborted;
    result.totalCleared = snapshot.cleared;
    result.queuedBuffers = snapshot.queued;
}

} // namespace

::media::Status MediaGraphRuntime::compile(MediaGraph graph)
{
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime compile failed: runtime is running"));
    }

    m_context.reset();
    m_scheduler.clear();
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

::media::Status MediaGraphRuntime::registerDefaultRuntimeNodes()
{
    if (!m_context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime registerDefaultRuntimeNodes failed: graph is not compiled"));
    }

    const MediaGraph* currentGraph = m_context.graph();
    if (!currentGraph) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime registerDefaultRuntimeNodes failed: graph is null"));
    }

    for (const MediaNode& node : currentGraph->nodes()) {
        if (m_scheduler.findNode(node.id)) {
            continue;
        }

        auto runtimeNode = MediaRuntimeNodeFactory::create(node);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }

        auto status = m_scheduler.registerNode(std::move(runtimeNode).value());
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
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

::media::Result<MediaGraphRunLoopResult> MediaGraphRuntime::runUntil(MediaGraphRunLoopOptions options,
                                                                      MediaGraphRunLoopStopPredicate stopPredicate)
{
    if (m_state != MediaGraphRuntimeState::Running) {
        return ::media::Result<MediaGraphRunLoopResult>::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime runUntil failed: runtime is not running"));
    }

    if (options.maxIterations == 0) {
        options.maxIterations = 1;
    }
    if (options.idleThreshold == 0) {
        options.idleThreshold = 1;
    }

    MediaGraphRunLoopResult result;
    ChannelActivitySnapshot previous = captureChannelActivity(m_context);
    copySnapshotToResult(previous, result);

    if (stopPredicate && stopPredicate(result)) {
        result.stoppedBecausePredicate = true;
        return ::media::Result<MediaGraphRunLoopResult>::success(result);
    }

    while (result.iterations < options.maxIterations) {
        auto status = processOnce();
        if (!status) {
            return ::media::Result<MediaGraphRunLoopResult>::failure(status.error());
        }

        ++result.iterations;

        const ChannelActivitySnapshot current = captureChannelActivity(m_context);
        copySnapshotToResult(current, result);

        const bool noActivity = sameChannelActivity(previous, current) && current.queued == 0;
        if (noActivity) {
            ++result.idleIterations;
        } else {
            result.idleIterations = 0;
        }

        previous = current;

        if (stopPredicate && stopPredicate(result)) {
            result.stoppedBecausePredicate = true;
            return ::media::Result<MediaGraphRunLoopResult>::success(result);
        }

        if (options.stopOnIdle && result.idleIterations >= options.idleThreshold) {
            result.stoppedBecauseIdle = true;
            return ::media::Result<MediaGraphRunLoopResult>::success(result);
        }
    }

    result.stoppedBecauseMaxIterations = true;
    return ::media::Result<MediaGraphRunLoopResult>::success(result);
}

::media::Result<MediaGraphRunLoopResult> MediaGraphRuntime::runUntilIdle(MediaGraphRunLoopOptions options)
{
    return runUntil(options, {});
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
