#include "internal/graph/runtime/compilation/MediaGraphRuntimeRegistrar.h"

#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/runtime/compilation/MediaDatagramServiceScopeAssembler.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/compilation/MediaDemuxClockRuntimeAssembler.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRuntimeRegistrar::registerDefaults(
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    std::vector<MediaPreparedRealtimeInputBinding>& inputBindings,
    std::vector<MediaAvRuntimeDomainState>& avDomains,
    const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
        protocolOutputAuthority)
{
    if (!context.compiled() || !context.graph()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime default registration requires compiled graph"));
    }
    std::vector<std::unique_ptr<MediaRuntimeNode>> preparedNodes;
    preparedNodes.reserve(context.graph()->nodes().size());
    struct SourceAssembly final {
        MediaAvSyncGroupKey groupKey;
        const MediaAvRuntimeInputRegistration* input;
        MediaNodeId preparationOwner;
        std::vector<MediaNodeId> members;
        std::optional<MediaPlaybackEpochActivationCapability>* activation;
        MediaAvReacquisitionAssemblyDependencies* reacquisition;
        std::shared_ptr<MediaAvStartupVideoPreparationState> preparation;
    };
    std::vector<SourceAssembly> sources;
    MediaAvOutputDomainRuntimeState* outputDomain = nullptr;
    std::optional<MediaAvSyncGroupKey> outputGroupKey;
    const MediaNode* videoOutputScheduler = nullptr;
    const MediaNode* mpegTsRtpSdpPublisher = nullptr;
    for (auto& domain : avDomains) {
        if (auto* shared = std::get_if<MediaAvSharedSourceOutputDomainRuntimeState>(&domain.role)) {
            std::vector<MediaNodeId> members;
            shared->registration.processing.forEach([&](MediaNodeId member) { members.push_back(member); });
            sources.push_back(SourceAssembly{domain.groupKey, &shared->registration.input,
                shared->registration.preparationOwner, std::move(members), &shared->activation,
                &shared->reacquisition, shared->videoPreparation});
            if (shared->registration.rtpSdpPublisher)
                mpegTsRtpSdpPublisher = context.graph()->findNode(*shared->registration.rtpSdpPublisher);
        } else if (auto* source = std::get_if<MediaAvSourceDomainRuntimeState>(&domain.role)) {
            sources.push_back(SourceAssembly{domain.groupKey, &source->registration.input,
                source->registration.preparationOwner, source->registration.processingMembers,
                &source->activation, &source->reacquisition, source->videoPreparation});
        } else {
            if (outputDomain) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "A/V registration rejects multiple continuous output domains"));
            outputDomain = &std::get<MediaAvOutputDomainRuntimeState>(domain.role);
            outputGroupKey = domain.groupKey;
            if (outputDomain->registration.rtpSdpPublisher)
                mpegTsRtpSdpPublisher = context.graph()->findNode(*outputDomain->registration.rtpSdpPublisher);
        }
    }
    if (avDomains.empty()) {
        for (const auto& node : context.graph()->nodes()) {
            if (node.kind == MediaNodeKind::VideoOutputScheduler) {
                if (videoOutputScheduler)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "VideoOnly runtime rejects duplicate output schedulers"));
                videoOutputScheduler = &node;
            }
            if (node.kind == MediaNodeKind::MpegTsRtpSdpPublisher) {
                if (mpegTsRtpSdpPublisher)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "MP2T SDP runtime rejects duplicate publishers"));
                mpegTsRtpSdpPublisher = &node;
            }
        }
    }
    for (const auto& source : sources) {
        const auto* sequencer = context.graph()->findNode(source.input->activationSequencer);
        const auto* extractor = context.graph()->findNode(source.input->releaseExtractor);
        const auto* owner = context.graph()->findNode(source.preparationOwner);
        if (!source.preparation || !sequencer || !extractor || !owner ||
            !*source.activation || scheduler.findNode(sequencer->id)) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Source domain requires its exact preparation roles and unused activation authority"));
        }
        if (auto status = source.preparation->bindSequencerWakeup(
                context.sharedNodeWakeup(sequencer->id)); !status) return status;
        if (auto status = source.preparation->bindOutputWakeup(
                context.sharedNodeWakeup(owner->id)); !status) return status;
        if (auto status = source.preparation->bindExtractorWakeup(
                context.sharedNodeWakeup(extractor->id)); !status) return status;
    }
    if (mpegTsRtpSdpPublisher) {
        auto sessionText = requiredNodeOption(
            &mpegTsRtpSdpPublisher->options,
            "MediaMpegTsRtpSdpPublisherNode",
            "mpegts_rtp_sdp.session");
        if (!sessionText) {
            return ::media::Status::failure(sessionText.error());
        }
        MediaProtocolOutputSessionKey sessionKey(
            std::move(sessionText).value());
        if (!protocolOutputAuthority || !sessionKey.valid() ||
            protocolOutputAuthority->sessionKey() != sessionKey ||
            !protocolOutputAuthority->sharedNtpEpoch()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MP2T SDP publisher requires its exact RTP output authority"));
        }
    }
    auto serviceScopes = MediaDatagramServiceScopeAssembler::assemble(
        *context.graph(), protocolOutputAuthority);
    if (!serviceScopes) return ::media::Status::failure(serviceScopes.error());
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer ||
            node.kind == MediaNodeKind::VideoOutputScheduler ||
            node.kind == MediaNodeKind::DemuxPacketClockBinder ||
            node.kind == MediaNodeKind::MpegTsRtpSdpPublisher)
            continue;
        if (outputDomain && node.id == outputDomain->registration.activationOwner) continue;
        if (scheduler.findNode(node.id)) continue;
        if (!MediaRuntimeNodeFactory::supported(node.kind)) {
            return ::media::Status::failure(::media::ErrorInfo::unsupported(
                "Default runtime registration encountered an unsupported planned node"));
        }
        MediaPreparedRealtimeInputBinding* binding = nullptr;
        for (auto& candidate : inputBindings) {
            if (candidate.nodeId == node.id) { binding = &candidate; break; }
        }
        std::shared_ptr<MediaDatagramServiceScopeArbiter> serviceScope;
        for (const auto& scope : serviceScopes.value()) {
            if (scope.sender == node.id) { serviceScope = scope.arbiter; break; }
        }
        std::shared_ptr<MediaAvStartupVideoPreparationState> preparation;
        for (const auto& source : sources) {
            if (node.id == source.input->releaseExtractor || node.id == source.preparationOwner) {
                if (preparation) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Runtime preparation consumer belongs to multiple source domains"));
                preparation = source.preparation;
            }
        }
        auto runtimeNode = MediaRuntimeNodeFactory::create(
            node, binding, preparation, protocolOutputAuthority, serviceScope);
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        if (outputDomain && node.kind == MediaNodeKind::CodecResolver) {
            auto* resolver = dynamic_cast<CodecResolverNode*>(runtimeNode.value().get());
            const auto* prepared = dynamic_cast<const FFmpegCodecContextBuffer*>(
                outputDomain->preparedVideoEncoder.get());
            if (!resolver || !prepared || !prepared->context() || !prepared->context()->hw_device_ctx)
                return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Composition registration requires its prepared encoder and shared hardware device"));
            const auto mode = node.options.value("codec_resolver.mode");
            const bool outputMember = std::find(outputDomain->registration.processingMembers.begin(),
                outputDomain->registration.processingMembers.end(), node.id) !=
                outputDomain->registration.processingMembers.end();
            if (mode != (outputMember ? "output_branch" : "source_decode"))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Composition codec resolver mode differs from its domain ownership"));
            auto bound = outputMember
                ? resolver->bindPreparedEncoder(outputDomain->preparedVideoEncoder)
                : resolver->bindPreparedHardwareDevice(prepared->context()->hw_device_ctx);
            if (!bound) return bound;
        }
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeNode,
                                "register node=" + std::to_string(node.id.value) +
                                    " name=" + node.name +
                                    " kind=" + mediaGraphDiagnosticNodeKindName(node.kind));
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (videoOutputScheduler) {
        if (scheduler.findNode(videoOutputScheduler->id)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoOnly scheduler is already registered without compiler injection"));
        }
        auto runtimeNode = MediaRuntimeNodeFactory::createVideoOutputScheduler(
            *videoOutputScheduler, protocolOutputAuthority);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    for (const auto& source : sources) {
        if (!source.input->demuxClock) continue;
        auto nodes = MediaDemuxClockRuntimeAssembler::create(
            context, source.groupKey, *source.input->demuxClock);
        if (!nodes) return ::media::Status::failure(nodes.error());
        for (auto& node : nodes.value()) preparedNodes.push_back(std::move(node));
    }
    if (mpegTsRtpSdpPublisher) {
        auto runtimeNode =
            MediaRuntimeNodeFactory::createMpegTsRtpSdpPublisher(
                *mpegTsRtpSdpPublisher, protocolOutputAuthority);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    for (const auto& source : sources) {
        auto runtimeNode = MediaRuntimeNodeFactory::createActivatedStartupReleaseSequencer(
            *context.graph()->findNode(source.input->activationSequencer),
            std::move(**source.activation), source.preparation);
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (outputDomain) {
        const auto* owner = context.graph()->findNode(outputDomain->registration.activationOwner);
        auto output = context.findAvSyncGroup(*outputGroupKey);
        if (!owner || !output || !outputDomain->activation || sources.empty() ||
            scheduler.findNode(owner->id)) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Continuous output requires its aggregate owner, sources and unused initial activation"));
        }
        std::vector<MediaAvAggregateSourceRuntime> inputs;
        for (const auto& planned : outputDomain->aggregatePlan->sources) {
            const auto found = std::find_if(sources.begin(), sources.end(), [&](const auto& source) {
                return source.groupKey == planned.groupKey;
            });
            if (found == sources.end()) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate plan names an unregistered source domain"));
            const auto& source = *found;
            auto group = context.findAvSyncGroup(source.groupKey);
            if (!group || group->clock() != output->clock()) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Aggregate sources and output must share the exact master clock"));
            }
            inputs.push_back(MediaAvAggregateSourceRuntime{std::move(group), source.preparation});
        }
        auto runtimeNode = MediaRuntimeNodeFactory::createContinuousAggregateNode(
            *owner, MediaAvAggregateRuntimeDependencies{
                std::move(inputs), std::move(output), std::move(*outputDomain->activation),
                outputDomain->aggregatePlan});
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    for (const auto& source : sources) {
        auto group = context.findAvSyncGroup(source.groupKey);
        const auto* transitionPlan = source.reacquisition->transitionService
            ? source.reacquisition->transitionService->transitionPlan() : nullptr;
        if (!group || !transitionPlan) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Source recovery requires its exact group and planned transition"));
        }
        auto assembler = MediaAvGenerationParticipantAssembler::create(*transitionPlan);
        if (!assembler) return ::media::Status::failure(assembler.error());
        for (auto& runtimeNode : preparedNodes) {
            if (std::find(source.members.begin(), source.members.end(), runtimeNode->nodeId()) == source.members.end()) continue;
            auto registration = MediaRuntimeNodeFactory::generationPurgeRegistration(*runtimeNode);
            if (!registration) continue;
            auto registered = assembler.value().registerTarget(
                registration->participant, std::move(registration->registration));
            if (!registered) return registered;
        }
        if (outputDomain) {
            const auto aggregate = std::find_if(preparedNodes.begin(), preparedNodes.end(), [&](const auto& node) {
                return node->nodeId() == outputDomain->registration.activationOwner;
            });
            if (aggregate == preparedNodes.end()) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Source purge registration requires its aggregate target"));
            auto registration = MediaRuntimeNodeFactory::generationPurgeRegistrationForSource(**aggregate, source.groupKey);
            if (!registration) return ::media::Status::failure(registration.error());
            auto registered = assembler.value().registerTarget(registration.value().participant,
                std::move(registration.value().registration));
            if (!registered) return registered;
        }
        auto participants = assembler.value().seal();
        if (!participants) return ::media::Status::failure(participants.error());
        std::vector<std::shared_ptr<MediaNodeWakeup>> wakeups;
        wakeups.reserve(source.members.size());
        for (const auto member : source.members) wakeups.push_back(context.sharedNodeWakeup(member));
        if (outputDomain) wakeups.push_back(context.sharedNodeWakeup(outputDomain->registration.activationOwner));
        auto coordinator = MediaAvReacquisitionCoordinator::create(
            source.groupKey, source.reacquisition->transitionService,
            source.reacquisition->masterClock, std::move(participants).value(), std::move(wakeups),
            *group->plan().sourceLifecycle,
            outputGroupKey ? context.findAvSyncGroup(*outputGroupKey) : nullptr);
        if (!coordinator) return ::media::Status::failure(coordinator.error());
        auto installed = group->installReacquisitionCoordinator(std::move(coordinator).value());
        if (!installed) return installed;
    }
    auto registered = scheduler.registerNodes(std::move(preparedNodes));
    if (!registered) return registered;
    for (const auto& source : sources) source.activation->reset();
    if (outputDomain) outputDomain->activation.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
