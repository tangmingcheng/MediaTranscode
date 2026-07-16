#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class ProductionAvSyncClockSource final : public MediaAvSyncClockSource {
public:
    ::media::Result<MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) override
    {
        auto masterClock = std::make_shared<MediaSteadyMasterClock>(
            MediaRunningTime::fromNanoseconds(0));
        std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch;
        if (requireSharedNtpEpoch) {
            auto masterNow = masterClock->now();
            if (!masterNow) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    masterNow.error());
            }
            const auto wallNow =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
            auto epoch = MediaSharedNtpEpoch::create(
                masterNow.value(), wallNow);
            if (!epoch) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    epoch.error());
            }
            sharedNtpEpoch = std::make_shared<const MediaSharedNtpEpoch>(
                std::move(epoch).value());
        }
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{std::move(masterClock),
                                   std::move(sharedNtpEpoch)});
    }
};

} // namespace

::media::Status MediaGraphRuntimeCompiler::validateBindings(const MediaRealtimeExecutableGraph& executable)
{
    std::unordered_set<std::uint64_t> bindingIds;
    std::size_t schedulerCount = 0;
    std::size_t binderCount = 0;
    std::size_t schedulerReferenceCount = 0;
    std::size_t binderReferenceCount = 0;
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
        if (node.kind == MediaNodeKind::AvOutputScheduler) ++schedulerCount;
        if (node.kind == MediaNodeKind::PlaybackEpochBinder) ++binderCount;
        if (node.kind == MediaNodeKind::RealtimeInput && !bindingIds.contains(node.id.value)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("MediaGraphRuntime missing prepared RealtimeInput binding"));
        }
        for (const auto& [key, value] : node.options.values()) {
            constexpr std::string_view SyncGroupSuffix = ".sync_group";
            constexpr std::string_view SchedulerGroupKey =
                "av_scheduler.sync_group";
            constexpr std::string_view BinderGroupKey =
                "playback_epoch_binder.sync_group";
            constexpr std::string_view StartupClockGroupKey =
                "av_startup_clock.sync_group";
            if (!key.ends_with(SyncGroupSuffix)) continue;
            const bool schedulerConsumer =
                node.kind == MediaNodeKind::AvOutputScheduler &&
                key == SchedulerGroupKey;
            const bool binderConsumer =
                node.kind == MediaNodeKind::PlaybackEpochBinder &&
                key == BinderGroupKey;
            const bool startupClockConsumer =
                node.kind == MediaNodeKind::AvStartupClock &&
                key == StartupClockGroupKey;
            if (!schedulerConsumer && !binderConsumer &&
                !startupClockConsumer) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime found an unsupported A/V sync group consumer"));
            }
            if (schedulerConsumer) ++schedulerReferenceCount;
            if (binderConsumer) ++binderReferenceCount;
            if (value.empty() || !executable.avSyncBinding) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime synchronized node requires an A/V sync binding"));
            }
            if (value != executable.avSyncBinding->groupKey.value()) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime synchronized node group does not match its binding"));
            }
        }
    }
    if (executable.avSyncBinding) {
        if (!executable.avSyncBinding->groupKey.valid()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime A/V sync binding has an invalid group"));
        }
        if (auto status = MediaAvSyncPlanValidator::validate(
                executable.avSyncBinding->plan); !status) {
            return status;
        }
        if (schedulerCount != 1 || binderCount != 1 ||
            schedulerReferenceCount != 1 || binderReferenceCount != 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime A/V sync binding requires exactly one scheduler and one playback epoch binder"));
        }
    } else if (schedulerCount != 0 || binderCount != 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Synchronized runtime nodes require an A/V sync binding"));
    }
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::compile(
    MediaRealtimeExecutableGraph executable,
    MediaGraph& activeGraph,
    std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
    std::optional<MediaPlaybackEpochActivationCapability>&
        playbackEpochActivationCapability,
    const std::shared_ptr<MediaAvSyncClockSource>& avSyncClockSource,
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    MediaGraphThreadedExecutor& threadedExecutor,
    MediaRuntimeAcceptanceCollector& acceptanceCollector,
    std::atomic_size_t& queueHighWatermark,
    MediaGraphRuntimeState& state)
{
    mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "compile.begin");
    if (auto valid = validateBindings(executable); !valid) {
        mediaGraphDiagnosticLog(
            context.diagnosticsEnabled(),
            MediaGraphDiagnosticPhase::RuntimeLifecycle,
            std::string("compile.failed error=") + valid.error().describe());
        return valid;
    }
    MediaGraphExecutionContext preparedContext;
    preparedContext.setDiagnosticConfig(context.diagnosticConfig());
    auto compiled = preparedContext.compile(executable.graph);
    if (!compiled) {
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                std::string("compile.failed error=") + compiled.error().describe());
        return compiled;
    }
    std::optional<MediaPlaybackEpochActivationCapability> preparedCapability;
    if (executable.avSyncBinding) {
        ProductionAvSyncClockSource productionClockSource;
        MediaAvSyncClockSource& clockSource = avSyncClockSource
            ? *avSyncClockSource
            : static_cast<MediaAvSyncClockSource&>(productionClockSource);
        auto clocks = MediaAvSyncRuntimeBootstrap::createClocks(
            *executable.avSyncBinding, clockSource);
        if (!clocks) {
            return ::media::Status::failure(clocks.error());
        }
        auto registered = MediaAvSyncRuntimeBootstrap::
            registerGroupAndIssueActivationCapability(
            *executable.avSyncBinding, std::move(clocks).value(),
            preparedContext);
        if (!registered) {
            return ::media::Status::failure(registered.error());
        }
        preparedCapability.emplace(std::move(registered).value());
    }
    const std::vector<MediaNodeId> oldExecutionOrder = context.executionOrder();
    context.shutdownAvSyncGroups();
    threadedExecutor.clear();
    scheduler.clear(oldExecutionOrder);
    activeGraph = std::move(executable.graph);
    preparedContext.rebindCompiledGraph(activeGraph);
    context = std::move(preparedContext);
    activeBindings = std::move(executable.inputBindings);
    playbackEpochActivationCapability = std::move(preparedCapability);
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
    std::vector<MediaPreparedRealtimeInputBinding>& inputBindings,
    std::optional<MediaPlaybackEpochActivationCapability>&
        playbackEpochActivationCapability)
{
    if (!context.compiled() || !context.graph()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime default registration requires compiled graph"));
    }
    std::vector<std::unique_ptr<MediaRuntimeNode>> preparedNodes;
    preparedNodes.reserve(context.graph()->nodes().size());
    const MediaNode* binder = nullptr;
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::PlaybackEpochBinder) {
            binder = &node;
            break;
        }
    }
    if (binder) {
        if (!playbackEpochActivationCapability) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Playback epoch binder is missing compiler-issued activation authority"));
        }
        if (scheduler.findNode(binder->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Playback epoch binder runtime node is already registered"));
        }
    } else if (playbackEpochActivationCapability) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Compiler-issued activation authority has no playback epoch binder"));
    }
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::PlaybackEpochBinder) continue;
        if (scheduler.findNode(node.id)) continue;
        if (!MediaRuntimeNodeFactory::supported(node.kind)) {
            return ::media::Status::failure(::media::ErrorInfo::unsupported(
                "Default runtime registration encountered an unsupported planned node"));
        }
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
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (binder && !scheduler.findNode(binder->id)) {
        auto runtimeNode = MediaRuntimeNodeFactory::createPlaybackEpochBinder(
            *binder, std::move(*playbackEpochActivationCapability));
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    auto registered = scheduler.registerNodes(std::move(preparedNodes));
    if (!registered) return registered;
    if (binder) playbackEpochActivationCapability.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
