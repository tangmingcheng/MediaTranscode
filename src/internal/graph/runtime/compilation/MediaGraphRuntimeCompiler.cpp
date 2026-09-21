#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"
#include "internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <chrono>
#include <string>
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

::media::Status MediaGraphRuntimeCompiler::validateBindings(
    const MediaRealtimeExecutableGraph& executable)
{
    std::unordered_set<std::uint64_t> bindingIds;
    for (const auto& binding : executable.inputBindings) {
        if (!binding.nodeId.isValid() || !binding.prepared.valid() ||
            !bindingIds.insert(binding.nodeId.value).second) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime duplicate or invalid prepared input binding"));
        }
        const MediaNode* node =
            executable.graph.findNode(binding.nodeId);
        const bool targetMatchesKind = node &&
            (binding.expectedKind == MediaPreparedRealtimeInputKind::RawRtp
                ? node->kind == MediaNodeKind::RawRtpInput
                : node->kind == MediaNodeKind::RealtimeInput);
        if (!targetMatchesKind) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime prepared binding target conflicts with expected input kind"));
        }
    }
    for (const MediaNode& node : executable.graph.nodes()) {
        if (node.kind == MediaNodeKind::RealtimeInput &&
            !bindingIds.contains(node.id.value)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime missing prepared RealtimeInput binding"));
        }
        if (node.kind == MediaNodeKind::RawRtpInput) {
            auto required = requiredBoolNodeOption(
                &node.options, "RawRtpInputNode",
                "rtp.prepared_input_required");
            if (!required) {
                return ::media::Status::failure(required.error());
            }
            const bool hasBinding = bindingIds.contains(node.id.value);
            if (required.value() != hasBinding) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        required.value()
                            ? "MediaGraphRuntime missing required prepared RawRtpInput binding"
                            : "MediaGraphRuntime node-owned RawRtpInput rejects prepared binding"));
            }
        }
    }
    if (const auto* avSyncBinding =
            std::get_if<MediaAvSyncRuntimeBinding>(
                &executable.runtimeBinding)) {
        if (auto absent =
                MediaRealtimeVideoGraphShapeValidator::validateAbsent(
                    executable.graph); !absent) {
            return absent;
        }
        return MediaAvSyncGraphShapeValidator::validate(
            executable.graph, *avSyncBinding);
    }
    if (const auto* videoBinding =
            std::get_if<MediaRealtimeVideoRuntimeBinding>(
                &executable.runtimeBinding)) {
        if (auto absent = MediaAvSyncGraphShapeValidator::validateAbsent(
                executable.graph); !absent) {
            return absent;
        }
        return MediaRealtimeVideoGraphShapeValidator::validate(
            executable.graph, *videoBinding);
    }
    if (!std::holds_alternative<MediaUnboundGraphRuntime>(
            executable.runtimeBinding)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MediaGraphRuntime encountered an unknown runtime binding variant"));
    }
    if (auto absent = MediaRealtimeVideoGraphShapeValidator::validateAbsent(
            executable.graph); !absent) {
        return absent;
    }
    return MediaAvSyncGraphShapeValidator::validateAbsent(executable.graph);
}

::media::Status MediaGraphRuntimeCompiler::compile(
    MediaRealtimeExecutableGraph executable,
    MediaGraph& activeGraph,
    std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
    std::vector<MediaAvRuntimeDomainState>& avDomains,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
        protocolOutputAuthority,
    const std::shared_ptr<MediaAvSyncClockSource>& avSyncClockSource,
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    MediaGraphThreadedExecutor& threadedExecutor,
    MediaRuntimeAcceptanceCollector& acceptanceCollector,
    std::atomic_size_t& queueHighWatermark,
    MediaGraphRuntimeState& state)
{
    mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "compile.begin");
    const bool requiresDefaultRegistration =
        !std::holds_alternative<MediaUnboundGraphRuntime>(
            executable.runtimeBinding);
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
    std::vector<MediaAvRuntimeDomainState> preparedDomains;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> preparedOutputAuthority;
    if (const auto* avSyncBinding =
            std::get_if<MediaAvSyncRuntimeBinding>(
                &executable.runtimeBinding)) {
        ProductionAvSyncClockSource productionClockSource;
        MediaAvSyncClockSource& clockSource = avSyncClockSource
            ? *avSyncClockSource
            : static_cast<MediaAvSyncClockSource&>(productionClockSource);
        auto clocks = MediaAvSyncRuntimeBootstrap::createClocks(
            *avSyncBinding, clockSource);
        if (!clocks) {
            return ::media::Status::failure(clocks.error());
        }
        bool outputRegistered = false;
        for (const auto& domain : avSyncBinding->domains) {
            const auto* shared = std::get_if<MediaAvSharedSourceOutputDomainBinding>(&domain.role);
            const auto* source = std::get_if<MediaAvSourceDomainBinding>(&domain.role);
            const bool output = std::holds_alternative<MediaAvOutputDomainBinding>(domain.role);
            if ((shared && avSyncBinding->domains.size() != 1) ||
                ((shared || output) != (domain.groupKey == avSyncBinding->outputGroupKey)) ||
                (outputRegistered && (shared || output))) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "A/V domains require exactly one explicitly identified output authority"));
            }
            auto registered = MediaAvSyncRuntimeBootstrap::registerGroupAndIssueActivationCapability(
                domain, clocks.value(), preparedContext);
            if (!registered) return ::media::Status::failure(registered.error());
            auto exactGroup = preparedContext.findAvSyncGroup(domain.groupKey);
            if (shared || output) {
                auto authority = MediaAvProtocolOutputRuntimeAuthority::create(exactGroup);
                if (!authority) return ::media::Status::failure(authority.error());
                preparedOutputAuthority = std::move(authority).value();
                outputRegistered = true;
            }
            if (output) {
                const auto& outputBinding = std::get<MediaAvOutputDomainBinding>(domain.role);
                if (!outputBinding.aggregatePlan) return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("Continuous output requires its typed aggregate plan"));
                preparedDomains.push_back(MediaAvRuntimeDomainState{
                    domain.groupKey, MediaAvOutputDomainRuntimeState{
                        std::get<MediaAvOutputDomainBinding>(domain.role).registration,
                        std::move(std::get<MediaOutputEpochActivationCapability>(registered.value())),
                        outputBinding.aggregatePlan, outputBinding.preparedVideoEncoder}});
                continue;
            }
            auto& activation = std::get<MediaPlaybackEpochActivationCapability>(registered.value());
            auto dependencies = MediaAvSyncRuntimeBootstrap::reacquisitionAssemblyDependencies(
                activation, exactGroup);
            if (!dependencies) return ::media::Status::failure(dependencies.error());
            auto preparation = shared ? shared->videoPreparationState : source->videoPreparationState;
            if (!preparation) {
                auto created = MediaAvStartupVideoPreparationState::create(domain.groupKey);
                if (!created) return ::media::Status::failure(created.error());
                preparation = std::move(created).value();
            }
            if (preparation->snapshot().groupKey != domain.groupKey) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "A/V domain preparation belongs to another group"));
            }
            if (shared) {
                preparedDomains.push_back(MediaAvRuntimeDomainState{
                    domain.groupKey, MediaAvSharedSourceOutputDomainRuntimeState{
                        shared->registration, std::move(activation),
                        std::move(dependencies).value(), std::move(preparation)}});
            } else {
                preparedDomains.push_back(MediaAvRuntimeDomainState{
                    domain.groupKey, MediaAvSourceDomainRuntimeState{
                        source->registration, std::move(activation),
                        std::move(dependencies).value(), std::move(preparation)}});
            }
        }
        if (!outputRegistered) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "A/V domains have no registered output authority"));
        }
    } else if (const auto* videoBinding =
                   std::get_if<MediaRealtimeVideoRuntimeBinding>(
                       &executable.runtimeBinding)) {
        auto authority = MediaVideoProtocolOutputRuntimeAuthority::create(
            videoBinding->runtime.sessionKey,
            videoBinding->runtime.scheduling.initialGeneration);
        if (!authority) {
            return ::media::Status::failure(authority.error());
        }
        preparedOutputAuthority = std::move(authority).value();
    }
    const std::vector<MediaNodeId> oldExecutionOrder = context.executionOrder();
    context.shutdownAvSyncGroups();
    threadedExecutor.clear();
    scheduler.clear(oldExecutionOrder);
    activeGraph = std::move(executable.graph);
    preparedContext.rebindCompiledGraph(activeGraph);
    context = std::move(preparedContext);
    activeBindings = std::move(executable.inputBindings);
    avDomains = std::move(preparedDomains);
    protocolOutputAuthority = std::move(preparedOutputAuthority);
    acceptanceCollector.reset();
    queueHighWatermark = 0;
    state = requiresDefaultRegistration
        ? MediaGraphRuntimeState::DefaultRegistrationPending
        : MediaGraphRuntimeState::Compiled;
    mediaGraphDiagnosticLog(
        context.diagnosticsEnabled(),
        MediaGraphDiagnosticPhase::RuntimeLifecycle,
        requiresDefaultRegistration
            ? "compile.done state=DefaultRegistrationPending"
            : "compile.done state=Compiled");
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::registerNode(
    MediaGraphScheduler& scheduler,
    std::unique_ptr<MediaRuntimeNode> node)
{
    return scheduler.registerNode(std::move(node));
}

} // namespace media::ffmpeg::graph
