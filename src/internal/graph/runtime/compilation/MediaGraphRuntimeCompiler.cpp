#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <string>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRuntimeCompiler::validateBindings(const MediaRealtimeExecutableGraph& executable)
{
    std::unordered_set<std::uint64_t> bindingIds;
    for (const auto& binding : executable.inputBindings) {
        if (!binding.nodeId.isValid() || !binding.prepared.valid() ||
            !bindingIds.insert(binding.nodeId.value).second) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MediaGraphRuntime duplicate or invalid prepared input binding"));
        }
        const MediaNode* node = executable.graph.findNode(binding.nodeId);
        if (!node || node->kind != MediaNodeKind::RealtimeInput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MediaGraphRuntime prepared binding target is not RealtimeInput"));
        }
    }
    for (const MediaNode& node : executable.graph.nodes()) {
        if (node.kind == MediaNodeKind::RealtimeInput && !bindingIds.contains(node.id.value)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("MediaGraphRuntime missing prepared RealtimeInput binding"));
        }
    }
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::compile(
    MediaGraph graph,
    std::vector<MediaPreparedRealtimeInputBinding> inputBindings,
    MediaGraph& activeGraph,
    std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    MediaGraphThreadedExecutor& threadedExecutor,
    MediaRuntimeAcceptanceCollector& acceptanceCollector,
    std::atomic_size_t& queueHighWatermark,
    MediaGraphRuntimeState& state)
{
    mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "compile.begin");
    MediaGraphExecutionContext preparedContext;
    preparedContext.setDiagnosticConfig(context.diagnosticConfig());
    auto compiled = preparedContext.compile(graph);
    if (!compiled) {
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                std::string("compile.failed error=") + compiled.error().describe());
        return compiled;
    }
    const std::vector<MediaNodeId> oldExecutionOrder = context.executionOrder();
    threadedExecutor.clear();
    scheduler.clear(oldExecutionOrder);
    activeGraph = std::move(graph);
    preparedContext.rebindCompiledGraph(activeGraph);
    context = std::move(preparedContext);
    activeBindings = std::move(inputBindings);
    acceptanceCollector.reset();
    queueHighWatermark = 0;
    state = MediaGraphRuntimeState::Compiled;
    mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "compile.done state=Compiled");
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::registerNode(
    MediaGraphScheduler& scheduler,
    std::unique_ptr<MediaRuntimeNode> node)
{
    return scheduler.registerNode(std::move(node));
}

::media::Status MediaGraphRuntimeCompiler::registerDefaults(
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    std::vector<MediaPreparedRealtimeInputBinding>& inputBindings)
{
    if (!context.compiled() || !context.graph()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime default registration requires compiled graph"));
    }
    for (const MediaNode& node : context.graph()->nodes()) {
        if (scheduler.findNode(node.id)) continue;
        MediaPreparedRealtimeInputBinding* binding = nullptr;
        for (auto& candidate : inputBindings) {
            if (candidate.nodeId == node.id) { binding = &candidate; break; }
        }
        auto runtimeNode = MediaRuntimeNodeFactory::create(node, binding);
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeNode,
                                "register node=" + std::to_string(node.id.value) +
                                    " name=" + node.name +
                                    " kind=" + mediaGraphDiagnosticNodeKindName(node.kind));
        auto registered = scheduler.registerNode(std::move(runtimeNode).value());
        if (!registered) return registered;
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
