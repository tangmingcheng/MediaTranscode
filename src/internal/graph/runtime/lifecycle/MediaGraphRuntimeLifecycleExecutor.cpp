#include "internal/graph/runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t CompletionIdleThreshold = 16;

struct ChannelActivitySnapshot final {
    std::uint64_t pushed = 0;
    std::uint64_t popped = 0;
    std::uint64_t closed = 0;
    std::uint64_t aborted = 0;
    std::uint64_t cleared = 0;
    std::size_t queued = 0;
};

ChannelActivitySnapshot capture(const MediaGraphExecutionContext& context)
{
    ChannelActivitySnapshot snapshot;
    for (const MediaChannel* channel : context.channels().channels()) {
        if (!channel) continue;
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

bool same(const ChannelActivitySnapshot& a, const ChannelActivitySnapshot& b) noexcept
{
    return a.pushed == b.pushed && a.popped == b.popped && a.closed == b.closed &&
           a.aborted == b.aborted && a.cleared == b.cleared && a.queued == b.queued;
}

void copy(const ChannelActivitySnapshot& snapshot, MediaGraphRunResult& result) noexcept
{
    result.totalPushed = snapshot.pushed;
    result.totalPopped = snapshot.popped;
    result.totalClosed = snapshot.closed;
    result.totalAborted = snapshot.aborted;
    result.totalCleared = snapshot.cleared;
    result.queuedBuffers = snapshot.queued;
}

std::string activity(const ChannelActivitySnapshot& snapshot)
{
    return "pushed=" + std::to_string(snapshot.pushed) +
           " popped=" + std::to_string(snapshot.popped) +
           " closed=" + std::to_string(snapshot.closed) +
           " aborted=" + std::to_string(snapshot.aborted) +
           " cleared=" + std::to_string(snapshot.cleared) +
           " queued=" + std::to_string(snapshot.queued);
}

} // namespace

::media::Result<MediaGraphRunResult> MediaGraphRuntimeLifecycleExecutor::run(MediaGraphRuntime& runtime)
{
    if (!runtime.m_context.compiled()) return ::media::Result<MediaGraphRunResult>::failure(
        ::media::ErrorInfo::notInitialized("MediaGraphRuntime run failed: graph is not compiled"));
    if (runtime.m_state != MediaGraphRuntimeState::Compiled) return ::media::Result<MediaGraphRunResult>::failure(
        ::media::ErrorInfo::invalidArgument("MediaGraphRuntime run failed: runtime is not compiled and ready"));
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "start.begin mode=single_thread");
    auto started = runtime.m_scheduler.start(runtime.m_context);
    if (!started) return ::media::Result<MediaGraphRunResult>::failure(started.error());
    runtime.m_state = MediaGraphRuntimeState::Running;
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "start.done state=Running");

    MediaGraphRunResult result;
    ChannelActivitySnapshot previous = capture(runtime.m_context);
    copy(previous, result);
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "run.begin " + activity(previous));
    for (;;) {
        auto step = runtime.m_scheduler.processSchedulingStep(runtime.m_context);
        if (!step) {
            (void)stop(runtime);
            return ::media::Result<MediaGraphRunResult>::failure(step.error());
        }
        ++result.iterations;
        const ChannelActivitySnapshot current = capture(runtime.m_context);
        copy(current, result);
        if (same(previous, current) && current.queued == 0) {
            ++result.idleIterations;
            if (result.idleIterations >= CompletionIdleThreshold) {
                result.completed = true;
                mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                        "run.completed iterations=" + std::to_string(result.iterations) +
                                            " idle_iterations=" + std::to_string(result.idleIterations) + " " + activity(current));
                auto stopped = stop(runtime);
                return stopped ? ::media::Result<MediaGraphRunResult>::success(result)
                               : ::media::Result<MediaGraphRunResult>::failure(stopped.error());
            }
        } else {
            result.idleIterations = 0;
        }
        previous = current;
    }
}

::media::Status MediaGraphRuntimeLifecycleExecutor::startThreaded(MediaGraphRuntime& runtime)
{
    if (!runtime.m_context.compiled()) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("MediaGraphRuntime startThreaded failed: graph is not compiled"));
    if (runtime.m_state != MediaGraphRuntimeState::Compiled) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("MediaGraphRuntime startThreaded failed: runtime is not compiled and ready"));
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "start.begin mode=threaded");
    runtime.m_threadedExecutor.setPolicy(runtime.m_threadingPolicy);
    auto started = runtime.m_threadedExecutor.start(runtime.m_context, runtime.m_scheduler);
    if (!started) return started;
    runtime.m_state = MediaGraphRuntimeState::ThreadedRunning;
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "start.done state=ThreadedRunning");
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeLifecycleExecutor::flush(MediaGraphRuntime& runtime)
{
    if (!runtime.m_context.compiled()) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("MediaGraphRuntime flush failed: graph is not compiled"));
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "flush.begin");
    auto status = runtime.m_scheduler.flush(runtime.m_context);
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            status ? "flush.done" : std::string("flush.failed error=") + status.error().describe());
    return status;
}

::media::Status MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(MediaGraphRuntime& runtime)
{
    if (runtime.m_state != MediaGraphRuntimeState::ThreadedRunning || !runtime.m_threadedExecutor.failed()) return ::media::Status::success();
    runtime.m_threadedExecutor.abort(runtime.m_context, runtime.m_scheduler);
    runtime.m_context.shutdownAvSyncGroups();
    runtime.m_playbackEpochActivationCapability.reset();
    runtime.m_state = MediaGraphRuntimeState::Aborted;
    return ::media::Status::failure(::media::ErrorInfo::internalError("MediaGraphRuntime threaded worker failed; runtime aborted"));
}

::media::Status MediaGraphRuntimeLifecycleExecutor::stop(MediaGraphRuntime& runtime)
{
    if (runtime.m_state != MediaGraphRuntimeState::Running && runtime.m_state != MediaGraphRuntimeState::ThreadedRunning) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MediaGraphRuntime stop failed: runtime is not running"));
    }
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "stop.begin");
    auto schedulerStatus = runtime.m_state == MediaGraphRuntimeState::ThreadedRunning
        ? runtime.m_threadedExecutor.stop(runtime.m_context, runtime.m_scheduler)
        : runtime.m_scheduler.stop(runtime.m_context);
    auto closeStatus = MediaGraphLifecycle::closeChannels(runtime.m_context);
    runtime.m_context.shutdownAvSyncGroups();
    runtime.m_playbackEpochActivationCapability.reset();
    if (!schedulerStatus) {
        if (runtime.m_threadedExecutor.state() == MediaGraphThreadedExecutorState::Aborted) {
            MediaGraphLifecycle::abortChannels(runtime.m_context);
            runtime.m_state = MediaGraphRuntimeState::Aborted;
        }
        return schedulerStatus;
    }
    if (!closeStatus) return closeStatus;
    runtime.m_state = MediaGraphRuntimeState::Stopped;
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "stop.done state=Stopped");
    return ::media::Status::success();
}

void MediaGraphRuntimeLifecycleExecutor::abort(MediaGraphRuntime& runtime) noexcept
{
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "abort.begin");
    if (runtime.m_state == MediaGraphRuntimeState::ThreadedRunning) runtime.m_threadedExecutor.abort(runtime.m_context, runtime.m_scheduler);
    else runtime.m_scheduler.abort(runtime.m_context);
    MediaGraphLifecycle::abortChannels(runtime.m_context);
    runtime.m_context.shutdownAvSyncGroups();
    runtime.m_playbackEpochActivationCapability.reset();
    runtime.m_state = MediaGraphRuntimeState::Aborted;
    mediaGraphDiagnosticLog(runtime.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "abort.done state=Aborted");
}

void MediaGraphRuntimeLifecycleExecutor::reset(MediaGraphRuntime& runtime)
{
    const bool diagnostics = runtime.diagnosticsEnabled();
    if (runtime.m_state == MediaGraphRuntimeState::Running || runtime.m_state == MediaGraphRuntimeState::ThreadedRunning) abort(runtime);
    const std::vector<MediaNodeId> order = runtime.m_context.executionOrder();
    runtime.m_threadedExecutor.clear();
    runtime.m_context.reset();
    runtime.m_scheduler.clear(order);
    runtime.m_context.setDiagnosticsEnabled(diagnostics);
    runtime.m_graph.clear();
    runtime.m_inputBindings.clear();
    runtime.m_playbackEpochActivationCapability.reset();
    runtime.m_acceptanceCollector.reset();
    runtime.m_queueHighWatermark = 0;
    runtime.m_state = MediaGraphRuntimeState::Empty;
}

} // namespace media::ffmpeg::graph
