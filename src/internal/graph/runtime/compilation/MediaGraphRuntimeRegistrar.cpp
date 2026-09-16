#include "internal/graph/runtime/compilation/MediaGraphRuntimeRegistrar.h"

#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/runtime/compilation/MediaDatagramServiceScopeAssembler.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/compilation/MediaDemuxClockRuntimeAssembler.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRuntimeRegistrar::registerDefaults(
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    std::vector<MediaPreparedRealtimeInputBinding>& inputBindings,
    std::optional<MediaAvRuntimeDomainState>& avDomain,
    const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
        protocolOutputAuthority)
{
    if (!context.compiled() || !context.graph()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime default registration requires compiled graph"));
    }
    std::vector<std::unique_ptr<MediaRuntimeNode>> preparedNodes;
    preparedNodes.reserve(context.graph()->nodes().size());
    const auto* domainPlan = avDomain ? &avDomain->registration : nullptr;
    const auto* sequencer = domainPlan
        ? context.graph()->findNode(domainPlan->input.activationSequencer) : nullptr;
    const auto* avOutputScheduler = domainPlan
        ? context.graph()->findNode(domainPlan->outputScheduler) : nullptr;
    const auto* releaseExtractor = domainPlan
        ? context.graph()->findNode(domainPlan->input.releaseExtractor) : nullptr;
    const auto* readinessOwner = domainPlan
        ? context.graph()->findNode(domainPlan->preparationOwner) : nullptr;
    const auto videoPreparationState = avDomain
        ? avDomain->videoPreparation : nullptr;
    const MediaNode* videoOutputScheduler = nullptr;
    const MediaNode* mpegTsRtpSdpPublisher = domainPlan && domainPlan->rtpSdpPublisher
        ? context.graph()->findNode(*domainPlan->rtpSdpPublisher) : nullptr;
    if (!avDomain) {
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
    if (videoPreparationState) {
        if (!sequencer) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Video preparation state requires a sequencer node"));
        }
        if (auto bound = videoPreparationState->bindSequencerWakeup(
                context.sharedNodeWakeup(sequencer->id)); !bound) return bound;
        if (!readinessOwner) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Video preparation state requires exactly one planned output readiness owner"));
        }
        if (auto bound = videoPreparationState->bindOutputWakeup(
                context.sharedNodeWakeup(readinessOwner->id)); !bound) {
            return bound;
        }
        if (releaseExtractor) {
            if (auto bound = videoPreparationState->bindExtractorWakeup(
                    context.sharedNodeWakeup(releaseExtractor->id)); !bound)
                return bound;
        }
    }
    if (sequencer) {
        if (!avDomain || !avDomain->activation) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Activation release sequencer is missing compiler-issued activation authority"));
        }
        if (scheduler.findNode(sequencer->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer runtime node is already registered"));
        }
    } else if (avDomain && avDomain->activation) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Compiler-issued activation authority has no activation release sequencer"));
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
        const bool domainMember = domainPlan &&
            std::find(domainPlan->members.begin(), domainPlan->members.end(), node.id) !=
                domainPlan->members.end();
        auto runtimeNode = MediaRuntimeNodeFactory::create(
            node, binding, domainMember ? videoPreparationState : nullptr,
            protocolOutputAuthority, serviceScope);
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
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
    if (domainPlan && domainPlan->input.demuxClock) {
        auto nodes = MediaDemuxClockRuntimeAssembler::create(
            context, avDomain->groupKey, *domainPlan->input.demuxClock);
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
    std::shared_ptr<MediaAvSyncGroupRuntime> reacquisitionGroup;
    std::optional<MediaAvReacquisitionAssemblyDependencies>
        reacquisitionDependencies;
    if (avDomain && avDomain->activation) {
        if (!avOutputScheduler) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V reacquisition assembly requires the planned output scheduler"));
        }
        const auto& groupKey = avDomain->groupKey;
        reacquisitionGroup = context.findAvSyncGroup(groupKey);
        if (!reacquisitionGroup ||
            reacquisitionGroup->key() != groupKey) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V reacquisition assembly requires the exact registered sync group"));
        }
        reacquisitionDependencies.emplace(avDomain->reacquisition);
    }
    if (sequencer && !scheduler.findNode(sequencer->id)) {
        auto runtimeNode =
            MediaRuntimeNodeFactory::createActivatedStartupReleaseSequencer(
                *sequencer, std::move(*avDomain->activation),
                videoPreparationState);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (reacquisitionDependencies) {
        auto assembler = MediaAvGenerationParticipantAssembler::create(
            reacquisitionDependencies->transitionService->transitionPlan());
        if (!assembler) {
            return ::media::Status::failure(assembler.error());
        }
        for (auto& runtimeNode : preparedNodes) {
            if (std::find(domainPlan->members.begin(), domainPlan->members.end(),
                          runtimeNode->nodeId()) == domainPlan->members.end()) continue;
            auto registration =
                MediaRuntimeNodeFactory::generationPurgeRegistration(
                    *runtimeNode);
            if (!registration) continue;
            auto registered = assembler.value().registerTarget(
                registration->participant,
                std::move(registration->registration));
            if (!registered) return registered;
        }
        auto participants = assembler.value().seal();
        if (!participants) {
            return ::media::Status::failure(participants.error());
        }
        std::vector<std::shared_ptr<MediaNodeWakeup>> domainWakeups;
        domainWakeups.reserve(domainPlan->members.size());
        for (const auto member : domainPlan->members) {
            domainWakeups.push_back(context.sharedNodeWakeup(member));
        }
        auto coordinator = MediaAvReacquisitionCoordinator::create(
            reacquisitionGroup->key(),
            std::move(reacquisitionDependencies->transitionService),
            std::move(reacquisitionDependencies->masterClock),
            std::move(participants).value(), std::move(domainWakeups));
        if (!coordinator) {
            return ::media::Status::failure(coordinator.error());
        }
        auto installed = reacquisitionGroup->installReacquisitionCoordinator(
            std::move(coordinator).value());
        if (!installed) return installed;
    }
    auto registered = scheduler.registerNodes(std::move(preparedNodes));
    if (!registered) return registered;
    if (sequencer) avDomain->activation.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
