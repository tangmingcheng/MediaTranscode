#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"

#include <algorithm>
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

::media::Result<std::shared_ptr<MediaRuntimeBranch>>
MediaGraphRuntime::extractInitialBranch(
    std::uint64_t id, std::span<const MediaNodeId> nodes,
    std::shared_ptr<MediaRuntimeBranchResourceReservation> reservation,
    std::span<const MediaNodeId> retirementProducerIds)
{
    using Result = ::media::Result<std::shared_ptr<MediaRuntimeBranch>>;
    if (m_state != MediaGraphRuntimeState::Compiled || !id || !reservation || nodes.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial output extraction requires a registered, unstarted DAG and resource reservation"));
    }
    auto branch = std::shared_ptr<MediaRuntimeBranch>(new MediaRuntimeBranch());
    branch->m_id = id;
    if (retirementProducerIds.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "initial branch requires explicit shared metadata producer retirement prerequisites"));
    for (const auto producer : retirementProducerIds) {
        auto token = m_context.nodeExitToken(producer);
        if (!token || std::find(nodes.begin(), nodes.end(), producer) != nodes.end())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "initial branch retirement producer must be a compiled shared node"));
        branch->m_retirementPrerequisites.push_back(std::move(token));
    }
    branch->m_inputsAccountedBySession = true;
    branch->m_threadingPolicy = m_threadingPolicy;
    if (m_threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        m_threadingPolicy.maxWorkerThreads < nodes.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial output extraction requires its planned per-node worker budget"));
    }
    branch->m_resourceLease = std::move(reservation);
    std::vector<MediaRuntimeSegmentOutputBinding> upstreamInputs;
    for (const auto& edge : m_graph.edges()) {
        if (std::find(nodes.begin(), nodes.end(), edge.to.nodeId) == nodes.end() ||
            std::find(nodes.begin(), nodes.end(), edge.from.nodeId) != nodes.end()) continue;
        if (std::any_of(upstreamInputs.begin(), upstreamInputs.end(), [&edge](const auto& input) {
                return input.port().id == edge.from.portId;
            })) continue;
        auto exported = m_context.exportOutput(edge.from.portId);
        if (!exported) return Result::failure(exported.error());
        upstreamInputs.push_back(std::move(exported).value());
    }
    auto compiled = branch->m_context.compileSegment(
        std::make_shared<const MediaGraph>(m_graph), nodes, m_context, upstreamInputs);
    if (!compiled) return Result::failure(compiled.error());
    // Complete allocations before moving factory-bound runtime node ownership.
    for (auto* channel : branch->m_context.channels().channels()) {
        if (std::find(nodes.begin(), nodes.end(), channel->binding().from.nodeId) == nodes.end())
            branch->m_inputs.push_back(channel);
    }
    auto extracted = m_scheduler.takeNodes(nodes);
    if (!extracted) return Result::failure(extracted.error());
    auto registered = branch->m_scheduler.registerNodes(std::move(extracted).value());
    if (!registered) return Result::failure(registered.error());
    m_context.detachExecutionNodes(nodes);
    return Result::success(std::move(branch));
}

std::shared_ptr<MediaProtocolOutputRuntimeAuthority>
MediaGraphRuntime::protocolOutputAuthority() const noexcept
{
    return m_protocolOutputAuthority;
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
        m_videoPreparationState,
        m_protocolOutputAuthority,
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
    auto registered = MediaGraphRuntimeCompiler::registerDefaults(
        m_context, m_scheduler, m_inputBindings,
        m_playbackEpochActivationCapability, m_videoPreparationState,
        m_protocolOutputAuthority);
    if (!registered) {
        if (m_state ==
            MediaGraphRuntimeState::DefaultRegistrationPending) {
            abort();
        }
        return registered;
    }
    if (m_state ==
        MediaGraphRuntimeState::DefaultRegistrationPending) {
        m_state = MediaGraphRuntimeState::Compiled;
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
    const bool unstarted = m_state == MediaGraphRuntimeState::Compiled ||
        m_state == MediaGraphRuntimeState::DefaultRegistrationPending;
    MediaGraphRuntimeLifecycleExecutor::abort(*this);
    if (unstarted) m_context.cancelUnstartedExecution();
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

bool MediaGraphRuntime::threadedCompleted() const noexcept
{
    return threadedRunning() && m_threadedExecutor.completed();
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
