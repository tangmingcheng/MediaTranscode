#include "internal/graph/runtime/MediaGraphRuntime.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t kCompletionIdleThreshold = 16;

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
                          MediaGraphRunResult& result) noexcept
{
    result.totalPushed = snapshot.pushed;
    result.totalPopped = snapshot.popped;
    result.totalClosed = snapshot.closed;
    result.totalAborted = snapshot.aborted;
    result.totalCleared = snapshot.cleared;
    result.queuedBuffers = snapshot.queued;
}

std::string activityText(const ChannelActivitySnapshot& snapshot)
{
    std::ostringstream out;
    out << "pushed=" << snapshot.pushed
        << " popped=" << snapshot.popped
        << " closed=" << snapshot.closed
        << " aborted=" << snapshot.aborted
        << " cleared=" << snapshot.cleared
        << " queued=" << snapshot.queued;
    return out.str();
}

} // namespace

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

    mediaGraphDiagnosticLog(diagnosticsEnabled(),
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            "compile.begin");

    m_context.reset();
    m_scheduler.clear();
    m_graph = std::move(graph);

    auto status = m_context.compile(m_graph);
    if (!status) {
        m_state = MediaGraphRuntimeState::Empty;
        mediaGraphDiagnosticLog(diagnosticsEnabled(),
                                MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                std::string("compile.failed error=") + status.error().describe());
        return status;
    }

    m_state = MediaGraphRuntimeState::Compiled;
    mediaGraphDiagnosticLog(diagnosticsEnabled(),
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            "compile.done state=Compiled");
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

        mediaGraphDiagnosticLog(diagnosticsEnabled(),
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                "register node=" + std::to_string(node.id.value) +
                                    " name=" + node.name +
                                    " kind=" + mediaGraphDiagnosticNodeKindName(node.kind));

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

::media::Result<MediaGraphRunResult> MediaGraphRuntime::run()
{
    if (!m_context.compiled()) {
        return ::media::Result<MediaGraphRunResult>::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime run failed: graph is not compiled"));
    }

    if (m_state == MediaGraphRuntimeState::ThreadedRunning) {
        return ::media::Result<MediaGraphRunResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime run failed: threaded runtime is running"));
    }

    if (m_state == MediaGraphRuntimeState::Stopped ||
        m_state == MediaGraphRuntimeState::Aborted) {
        return ::media::Result<MediaGraphRunResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRuntime run failed: runtime has already been stopped or aborted; compile a new graph"));
    }

    if (m_state != MediaGraphRuntimeState::Running) {
        mediaGraphDiagnosticLog(diagnosticsEnabled(),
                                MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                "start.begin mode=single_thread");

        auto startStatus = m_scheduler.start(m_context);
        if (!startStatus) {
            return ::media::Result<MediaGraphRunResult>::failure(startStatus.error());
        }

        m_state = MediaGraphRuntimeState::Running;
        mediaGraphDiagnosticLog(diagnosticsEnabled(),
                                MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                "start.done state=Running");
    }

    MediaGraphRunResult result;
    ChannelActivitySnapshot previous = captureChannelActivity(m_context);
    copySnapshotToResult(previous, result);

    mediaGraphDiagnosticLog(diagnosticsEnabled(),
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            "run.begin " + activityText(previous));

    for (;;) {
        auto status = m_scheduler.processSchedulingStep(m_context);
        if (!status) {
            auto stopStatus = stop();
            (void)stopStatus;
            return ::media::Result<MediaGraphRunResult>::failure(status.error());
        }

        ++result.iterations;

        const ChannelActivitySnapshot current = captureChannelActivity(m_context);
        copySnapshotToResult(current, result);

        const bool noActivity = sameChannelActivity(previous, current) && current.queued == 0;
        if (noActivity) {
            ++result.idleIterations;
            if (result.idleIterations >= kCompletionIdleThreshold) {
                result.completed = true;
                mediaGraphDiagnosticLog(diagnosticsEnabled(),
                                        MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                        "run.completed iterations=" + std::to_string(result.iterations) +
                                            " idle_iterations=" + std::to_string(result.idleIterations) +
                                            " " + activityText(current));

                auto stopStatus = stop();
                if (!stopStatus) {
                    return ::media::Result<MediaGraphRunResult>::failure(stopStatus.error());
                }

                return ::media::Result<MediaGraphRunResult>::success(result);
            }
        } else {
            result.idleIterations = 0;
        }

        previous = current;
    }
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

    mediaGraphDiagnosticLog(diagnosticsEnabled(),
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            "start.begin mode=threaded");

    m_threadedExecutor.setPolicy(m_threadingPolicy);
    auto status = m_threadedExecutor.start(m_context, m_scheduler);
    if (!status) {
        return status;
    }

    m_state = MediaGraphRuntimeState::ThreadedRunning;
    mediaGraphDiagnosticLog(diagnosticsEnabled(),
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            "start.done state=ThreadedRunning");
    return ::media::Status::success();
}

::media::Status MediaGraphRuntime::flush()
{
    if (!m_context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime flush failed: graph is not compiled"));
    }

    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "flush.begin");
    auto status = m_scheduler.flush(m_context);
    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            status ? "flush.done" : std::string("flush.failed error=") + status.error().describe());
    return status;
}

::media::Status MediaGraphRuntime::stop()
{
    if (!m_context.compiled()) {
        m_state = MediaGraphRuntimeState::Stopped;
        return ::media::Status::success();
    }

    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "stop.begin");

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
    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "stop.done state=Stopped");
    return ::media::Status::success();
}

void MediaGraphRuntime::abort() noexcept
{
    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "abort.begin");
    if (m_state == MediaGraphRuntimeState::ThreadedRunning) {
        m_threadedExecutor.abort(m_context, m_scheduler);
    } else {
        m_scheduler.abort(m_context);
    }
    MediaGraphLifecycle::abortChannels(m_context);
    m_state = MediaGraphRuntimeState::Aborted;
    mediaGraphDiagnosticLog(diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "abort.done state=Aborted");
}

void MediaGraphRuntime::reset()
{
    const bool diagnosticsEnabledValue = diagnosticsEnabled();
    if (m_state == MediaGraphRuntimeState::Running ||
        m_state == MediaGraphRuntimeState::ThreadedRunning) {
        abort();
    }

    m_threadedExecutor.clear();
    m_scheduler.clear();
    m_context.reset();
    m_context.setDiagnosticsEnabled(diagnosticsEnabledValue);
    m_graph.clear();
    m_state = MediaGraphRuntimeState::Empty;
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

const MediaGraphThreadedExecutor& MediaGraphThreadedExecutor() const noexcept;

const MediaGraphThreadedExecutor& MediaGraphRuntime::threadedExecutor() const noexcept
{
    return m_threadedExecutor;
}

const MediaGraph* MediaGraphRuntime::graph() const noexcept
{
    return m_context.graph();
}

} // namespace media::ffmpeg::graph
