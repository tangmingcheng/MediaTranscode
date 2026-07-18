#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

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
    constexpr std::string_view SyncGroupSuffix = ".sync_group";
    constexpr std::string_view SchedulerGroupKey = "av_scheduler.sync_group";
    constexpr std::string_view BinderGroupKey = "playback_epoch_binder.sync_group";
    constexpr std::string_view StartupClockGroupKey = "av_startup_clock.sync_group";
    constexpr std::string_view SequencerGroupKey =
        "activated_startup_release_sequencer.sync_group";
    constexpr std::string_view RtpBinderGroupKey = "rtp_clock_binder.sync_group";
    constexpr std::string_view InitialGateGroupKey = "initial_locked_gate.sync_group";
    constexpr std::string_view CoordinatorGroupKey = "av_startup.sync_group";
    constexpr std::string_view AudioDriftControllerGroupKey =
        "audio_drift_controller.sync_group";
    constexpr std::string_view ScheduledRtpSenderGroupKey =
        "scheduled_rtp.sync_group";
    constexpr std::string_view ScheduledTsAdapterGroupKey =
        "scheduled_ts_adapter.sync_group";
    constexpr std::string_view ProjectMpegTsPlanGroupKey =
        "project_mpeg_ts_plan.sync_group";
    std::unordered_set<std::uint64_t> bindingIds;
    std::size_t schedulerCount = 0;
    std::size_t binderCount = 0;
    std::size_t sequencerCount = 0;
    std::size_t scheduledRtpSenderCount = 0;
    std::size_t scheduledTsAdapterCount = 0;
    std::size_t projectMpegTsPlanSourceCount = 0;
    std::size_t dualMediaSdpPublisherCount = 0;
    std::size_t schedulerReferenceCount = 0;
    std::size_t binderReferenceCount = 0;
    std::size_t sequencerReferenceCount = 0;
    std::size_t scheduledRtpSenderReferenceCount = 0;
    std::size_t scheduledTsAdapterReferenceCount = 0;
    std::size_t projectMpegTsPlanSourceReferenceCount = 0;
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
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer)
            ++sequencerCount;
        if (node.kind == MediaNodeKind::ScheduledRtpSender)
            ++scheduledRtpSenderCount;
        if (node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter)
            ++scheduledTsAdapterCount;
        if (node.kind == MediaNodeKind::ProjectMpegTsPlanSource)
            ++projectMpegTsPlanSourceCount;
        if (node.kind == MediaNodeKind::DualMediaSdpPublisher)
            ++dualMediaSdpPublisherCount;
        if (node.kind == MediaNodeKind::RealtimeInput && !bindingIds.contains(node.id.value)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("MediaGraphRuntime missing prepared RealtimeInput binding"));
        }
        if (node.kind == MediaNodeKind::AudioDriftController &&
            !node.options.has(std::string(AudioDriftControllerGroupKey))) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime audio drift controller requires its planned sync group"));
        }
        for (const auto& [key, value] : node.options.values()) {
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
            const bool sequencerConsumer =
                node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer &&
                key == SequencerGroupKey;
            const bool rtpBinderConsumer =
                node.kind == MediaNodeKind::RtpPacketClockBinder &&
                key == RtpBinderGroupKey;
            const bool initialGateConsumer =
                node.kind == MediaNodeKind::InitialLockedPacketGate &&
                key == InitialGateGroupKey;
            const bool coordinatorConsumer =
                node.kind == MediaNodeKind::AvStartupCoordinator &&
                key == CoordinatorGroupKey;
            const bool audioDriftControllerConsumer =
                node.kind == MediaNodeKind::AudioDriftController &&
                key == AudioDriftControllerGroupKey;
            const bool scheduledRtpSenderConsumer =
                node.kind == MediaNodeKind::ScheduledRtpSender &&
                key == ScheduledRtpSenderGroupKey;
            const bool scheduledTsAdapterConsumer =
                node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter &&
                key == ScheduledTsAdapterGroupKey;
            const bool projectMpegTsPlanSourceConsumer =
                node.kind == MediaNodeKind::ProjectMpegTsPlanSource &&
                key == ProjectMpegTsPlanGroupKey;
            if (!schedulerConsumer && !binderConsumer &&
                !startupClockConsumer && !sequencerConsumer &&
                !rtpBinderConsumer && !initialGateConsumer &&
                !coordinatorConsumer && !audioDriftControllerConsumer &&
                !scheduledRtpSenderConsumer &&
                !scheduledTsAdapterConsumer &&
                !projectMpegTsPlanSourceConsumer) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime found an unsupported A/V sync group consumer"));
            }
            if (schedulerConsumer) ++schedulerReferenceCount;
            if (binderConsumer) ++binderReferenceCount;
            if (sequencerConsumer) ++sequencerReferenceCount;
            if (scheduledRtpSenderConsumer)
                ++scheduledRtpSenderReferenceCount;
            if (scheduledTsAdapterConsumer)
                ++scheduledTsAdapterReferenceCount;
            if (projectMpegTsPlanSourceConsumer)
                ++projectMpegTsPlanSourceReferenceCount;
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
        if (schedulerCount != 1 || binderCount != 1 || sequencerCount != 1 ||
            schedulerReferenceCount != 1 || binderReferenceCount != 1 ||
            sequencerReferenceCount != 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime A/V sync binding requires exactly one scheduler, binder, and activation release sequencer"));
        }
        const bool hasScheduledRtpOutput = scheduledRtpSenderCount != 0 ||
            dualMediaSdpPublisherCount != 0;
        if (hasScheduledRtpOutput &&
            (scheduledRtpSenderCount != 2 ||
             scheduledRtpSenderReferenceCount != 2 ||
             dualMediaSdpPublisherCount != 1)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime scheduled RTP output requires exactly two injected senders and one SDP publisher"));
        }
        if (scheduledTsAdapterReferenceCount != scheduledTsAdapterCount) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime scheduled MPEG-TS adapters require their exact planned sync group"));
        }
        if (projectMpegTsPlanSourceReferenceCount !=
            projectMpegTsPlanSourceCount) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime project MPEG-TS plan sources require their exact planned sync group"));
        }
    } else if (schedulerCount != 0 || binderCount != 0 ||
               sequencerCount != 0 || scheduledRtpSenderCount != 0 ||
               dualMediaSdpPublisherCount != 0 ||
               scheduledTsAdapterCount != 0 ||
               projectMpegTsPlanSourceCount != 0) {
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
    const MediaNode* sequencer = nullptr;
    std::vector<const MediaNode*> scheduledRtpSenders;
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer) {
            sequencer = &node;
        }
        if (node.kind == MediaNodeKind::ScheduledRtpSender)
            scheduledRtpSenders.push_back(&node);
    }
    if (sequencer) {
        if (!playbackEpochActivationCapability) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Activation release sequencer is missing compiler-issued activation authority"));
        }
        if (scheduler.findNode(sequencer->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer runtime node is already registered"));
        }
    } else if (playbackEpochActivationCapability) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Compiler-issued activation authority has no activation release sequencer"));
    }
    std::shared_ptr<MediaAvSyncGroupRuntime> scheduledRtpGroup;
    for (const MediaNode* sender : scheduledRtpSenders) {
        if (!sender) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "Scheduled RTP sender registration lost its planned node"));
        }
        if (scheduler.findNode(sender->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender runtime node is already registered without compiler injection"));
        }
        auto groupText = requiredNodeOption(
            &sender->options, "MediaScheduledRtpSenderNode",
            "scheduled_rtp.sync_group");
        if (!groupText) return ::media::Status::failure(groupText.error());
        MediaAvSyncGroupKey groupKey(std::move(groupText).value());
        auto exactGroup = context.findAvSyncGroup(groupKey);
        if (!exactGroup || exactGroup->key() != groupKey) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender compiler injection cannot find its exact registered sync group"));
        }
        if (scheduledRtpGroup && scheduledRtpGroup != exactGroup) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP senders do not share the exact registered sync-group runtime"));
        }
        scheduledRtpGroup = std::move(exactGroup);
    }
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer ||
            node.kind == MediaNodeKind::ScheduledRtpSender)
            continue;
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
    for (const MediaNode* sender : scheduledRtpSenders) {
        auto runtimeNode = MediaRuntimeNodeFactory::createScheduledRtpSender(
            *sender, scheduledRtpGroup);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (sequencer && !scheduler.findNode(sequencer->id)) {
        auto runtimeNode =
            MediaRuntimeNodeFactory::createActivatedStartupReleaseSequencer(
                *sequencer, std::move(*playbackEpochActivationCapability));
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    auto registered = scheduler.registerNodes(std::move(preparedNodes));
    if (!registered) return registered;
    if (sequencer) playbackEpochActivationCapability.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
