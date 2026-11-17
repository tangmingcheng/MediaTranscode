#include "internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeRegistrar.h"

#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/runtime/compilation/MediaDatagramServiceScopeAssembler.h"
#include "internal/graph/planner/realtime/MediaNetworkStorageWakeupPlanner.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/compilation/MediaDemuxClockRuntimeAssembler.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/nodes/input/RawRtpInputNode.h"
#include "internal/graph/nodes/video/HardwareTransferNode.h"
#include "internal/graph/nodes/input/MediaRtpControlOutputNode.h"
#include "internal/graph/runtime/resource/MediaSourceReferenceAllocation.h"
#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"
#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

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
        std::shared_ptr<MediaPreparedVideoDecoder> decoder;
        std::shared_ptr<MediaPreparedRtpSourceSession> rtpSource;
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
                &shared->reacquisition, shared->videoPreparation, nullptr, nullptr});
            if (shared->registration.rtpSdpPublisher)
                mpegTsRtpSdpPublisher = context.graph()->findNode(*shared->registration.rtpSdpPublisher);
        } else if (auto* source = std::get_if<MediaAvSourceDomainRuntimeState>(&domain.role)) {
            sources.push_back(SourceAssembly{domain.groupKey, &source->registration.input,
                source->registration.preparationOwner, source->registration.processingMembers,
                &source->activation, &source->reacquisition, source->videoPreparation,
                source->preparedVideoDecoder, source->preparedRtpSource});
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
        if (source.input->rtpClock) {
            const auto& rtp = *source.input->rtpClock;
            const bool shared = rtp.controlDelivery == MediaRtpControlDelivery::SharedPreparedSession;
            if ((!shared && rtp.controlDelivery != MediaRtpControlDelivery::RawProtocolOwner) ||
                shared != static_cast<bool>(source.rtpSource) ||
                rtp.videoControl.has_value() != shared ||
                rtp.audioControl.has_value() != (shared && rtp.audioRaw.has_value()))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "RTP control delivery differs from the exact prepared session ownership"));
        } else if (source.rtpSource) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Prepared RTP session lacks its typed control delivery registration"));
        }
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
        if (outputDomain && (node.id == outputDomain->registration.activationOwner ||
            node.id == outputDomain->registration.audioAggregate)) continue;
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
        std::optional<MediaNetworkStorageWakeupPlan> storageWakeups;
        if (node.kind == MediaNodeKind::DatagramTransportPlanSource) {
            auto planned = MediaNetworkStorageWakeupPlanner::plan(*context.graph(), node.id);
            if (!planned) return ::media::Status::failure(planned.error());
            storageWakeups = std::move(planned).value();
        }
        std::shared_ptr<MediaAvStartupVideoPreparationState> preparation;
        std::shared_ptr<MediaAvSyncGroupRuntime> preparationGroup;
        std::optional<MediaLineageAllocationScope> preparationAllocation;
        for (const auto& source : sources) {
            if (node.id == source.input->releaseExtractor || node.id == source.preparationOwner) {
                if (preparation) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Runtime preparation consumer belongs to multiple source domains"));
                preparation = source.preparation;
                preparationGroup = context.findAvSyncGroup(source.groupKey);
                if (node.kind == MediaNodeKind::VideoPreparation) {
                    if (!preparationGroup) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                        "Video preparation has no registered source group"));
                    if (preparationGroup->plan().startup.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan) {
                        if (!source.rtpSource || !source.rtpSource->videoReferenceResource())
                            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                                "Video preparation has no prepared source allocation owner"));
                        preparationAllocation.emplace(MediaPreparedLineageAllocation{source.rtpSource->videoReferenceResource()});
                    } else if (preparationGroup->plan().startup.videoReadiness == MediaVideoStartupReadiness::IntervalCoverage) {
                        preparationAllocation.emplace(MediaRuntimeLineageAllocation{});
                    } else {
                        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                            "Video preparation has no planned source readiness"));
                    }
                }
            }
        }
        std::shared_ptr<MediaAvSyncGroupRuntime> decodedStartupGroup;
        std::shared_ptr<MediaPreparationMemoryResource> decodedStartupAllocation;
        if (node.kind == MediaNodeKind::DecodedVideoStartup || node.kind == MediaNodeKind::CanonicalInput ||
            node.kind == MediaNodeKind::AvStartupCoordinator ||
            node.kind == MediaNodeKind::PlaybackEpochBinder) {
            bool foundSource = false;
            for (const auto& source : sources) {
                if (std::find(source.members.begin(), source.members.end(), node.id) == source.members.end()) continue;
                if (foundSource) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Decoded startup belongs to multiple source domains"));
                foundSource = true;
                decodedStartupGroup = context.findAvSyncGroup(source.groupKey);
                if (node.kind != MediaNodeKind::DecodedVideoStartup && decodedStartupGroup &&
                    decodedStartupGroup->plan().startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan)
                    continue;
                if (!source.rtpSource) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Decoded startup has no prepared source allocation owner"));
                decodedStartupAllocation = source.rtpSource->videoReferenceResource();
            }
        }
        if (node.kind == MediaNodeKind::VideoPreparation && !preparationAllocation)
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Video preparation has no explicit source allocation domain"));
        auto runtimeNode = node.kind == MediaNodeKind::DecodedVideoStartup
            ? MediaRuntimeNodeFactory::createDecodedVideoStartup(node, std::move(decodedStartupGroup),
                std::move(decodedStartupAllocation))
            : node.kind == MediaNodeKind::CanonicalInput && decodedStartupAllocation
            ? MediaRuntimeNodeFactory::createCanonicalInput(node,
                MediaPreparedLineageAllocation{std::move(decodedStartupAllocation)})
            : node.kind == MediaNodeKind::AvStartupCoordinator && decodedStartupAllocation
            ? MediaRuntimeNodeFactory::createStartupCoordinator(node,
                MediaPreparedLineageAllocation{std::move(decodedStartupAllocation)})
            : node.kind == MediaNodeKind::PlaybackEpochBinder && decodedStartupAllocation
            ? MediaRuntimeNodeFactory::createStartupBinder(node,
                MediaPreparedLineageAllocation{std::move(decodedStartupAllocation)})
            : node.kind == MediaNodeKind::VideoPreparation
            ? MediaRuntimeNodeFactory::createVideoPreparation(
                node, std::move(preparationGroup), preparation, std::move(*preparationAllocation))
            : MediaRuntimeNodeFactory::create(
                node, binding, preparation, protocolOutputAuthority, serviceScope,
                storageWakeups ? &*storageWakeups : nullptr,
                context.graph()->protocolGenerationStorageFor(node.id));
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        if (auto* consumer = dynamic_cast<MediaSourceReferenceAllocation*>(runtimeNode.value().get())) {
            const SourceAssembly* owner = nullptr;
            for (const auto& source : sources) {
                if (std::find(source.members.begin(), source.members.end(), node.id) == source.members.end()) continue;
                if (owner) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Source frame processor belongs to multiple source domains"));
                owner = &source;
            }
            if (owner) {
                auto group = context.findAvSyncGroup(owner->groupKey);
                if (!group) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Source frame processor has no registered source group"));
                if (auto* transfer = dynamic_cast<HardwareTransferNode*>(runtimeNode.value().get())) {
                    if (auto status = transfer->bindSourceGroup(group, owner->preparation); !status) return status;
                }
                if (!owner->rtpSource && group->plan().startup.videoReadiness ==
                        MediaVideoStartupReadiness::DecodedPresentationSpan)
                    return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                        "Source frame processor has no prepared allocation owner"));
                if (owner->rtpSource) {
                    if (auto status = consumer->bindSourceReferenceResource(
                            owner->rtpSource->videoReferenceResource()); !status) return status;
                }
            }
        }
        for (const auto& source : sources) {
            if (!source.rtpSource) continue;
            if (!source.input->rtpClock || source.input->demuxClock)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP source requires its typed RTP registration"));
            const auto& rtp = *source.input->rtpClock;
            if (rtp.audioRaw.has_value() != rtp.audioBinder.has_value() ||
                rtp.audioRaw.has_value() != source.rtpSource->plan().audio.has_value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration has inconsistent stream members"));
            auto group = context.findAvSyncGroup(source.groupKey);
            if (!group || group->clock() != source.rtpSource->clock())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration requires its original master clock"));
            if (node.id == rtp.videoRaw || (rtp.audioRaw && node.id == *rtp.audioRaw)) {
                auto* raw = dynamic_cast<RawRtpInputNode*>(runtimeNode.value().get());
                if (!raw) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration does not name a raw input"));
                if (node.id == rtp.videoRaw) {
                    if (auto status = source.rtpSource->bindRuntimeGroup(group); !status) return status;
                }
                if (auto status = raw->bindPreparedSession(source.rtpSource); !status) return status;
            } else if ((rtp.videoControl && node.id == *rtp.videoControl) ||
                       (rtp.audioControl && node.id == *rtp.audioControl)) {
                auto* control = dynamic_cast<MediaRtpControlOutputNode*>(runtimeNode.value().get());
                if (!control) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration does not name a control output"));
                auto endpoint = MediaPreparedRtpControlEndpoint::create(source.rtpSource,
                    node.id == *rtp.videoControl ? MediaStreamKind::Video : MediaStreamKind::Audio);
                if (!endpoint) return ::media::Status::failure(endpoint.error());
                if (auto status = control->bindEndpoint(std::move(endpoint).value()); !status) return status;
            } else if (node.id == rtp.clockGroup) {
                auto* clock = dynamic_cast<MediaRtpClockGroupNode*>(runtimeNode.value().get());
                if (!clock) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration does not name a clock group"));
                if (auto status = clock->bindPreparedSession(source.rtpSource); !status) return status;
            } else if (node.id == rtp.videoBinder || (rtp.audioBinder && node.id == *rtp.audioBinder)) {
                auto* binder = dynamic_cast<MediaRtpPacketClockBinderNode*>(runtimeNode.value().get());
                if (!binder) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP registration does not name a packet clock binder"));
                if (auto status = binder->bindPreparedSession(source.rtpSource); !status) return status;
            }
        }
        if (outputDomain && node.kind == MediaNodeKind::CodecResolver) {
            auto* resolver = dynamic_cast<CodecResolverNode*>(runtimeNode.value().get());
            const auto* prepared = dynamic_cast<const FFmpegCodecContextBuffer*>(
                outputDomain->preparedVideoEncoder.get());
            if (!resolver || !prepared || !prepared->context())
                return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Composition registration requires its actual prepared encoder"));
            const auto mode = node.options.value("codec_resolver.mode");
            const bool outputMember = std::find(outputDomain->registration.processingMembers.begin(),
                outputDomain->registration.processingMembers.end(), node.id) !=
                outputDomain->registration.processingMembers.end();
            if (mode != (outputMember ? "output_branch" : "source_decode"))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Composition codec resolver mode differs from its domain ownership"));
            std::shared_ptr<MediaPreparedVideoDecoder> decoder;
            if (!outputMember) {
                for (const auto& source : sources) {
                    if (std::find(source.members.begin(), source.members.end(), node.id) == source.members.end()) continue;
                    if (decoder || !source.decoder) return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument("Source resolver lacks a unique prepared decoder owner"));
                    decoder = source.decoder;
                }
                if (!decoder) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Source resolver was not prepared before DAG registration"));
                if (auto device = decoder->validateHardwareDevice(prepared->context()->hw_device_ctx); !device)
                    return device;
            }
            auto bound = outputMember
                ? resolver->bindPreparedEncoder(outputDomain->preparedVideoEncoder)
                : resolver->bindPreparedDecoder(std::move(decoder));
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
        if (!owner || !output || !outputDomain->activation || !outputDomain->preparedCanvas || sources.empty() ||
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
        auto coordination = std::make_shared<MediaAvAggregateCoordination>(*outputDomain->aggregatePlan);
        outputDomain->coordination = coordination;
        const auto* audioNode = context.graph()->findNode(outputDomain->registration.audioAggregate);
        if (!audioNode || audioNode->kind != MediaNodeKind::ContinuousAudioAggregate)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Continuous output lacks its audio aggregate"));
        preparedNodes.push_back(std::make_unique<MediaContinuousAudioAggregateNode>(audioNode->id,
            MediaContinuousAudioAggregateDependencies{
                inputs[outputDomain->aggregatePlan->audioSource].group, output,
                outputDomain->aggregatePlan, coordination}));
        auto runtimeNode = MediaRuntimeNodeFactory::createContinuousAggregateNode(
            *owner, MediaAvAggregateRuntimeDependencies{
                std::move(inputs), std::move(output), std::move(*outputDomain->activation),
                outputDomain->aggregatePlan, outputDomain->preparedCanvas, std::move(coordination)});
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
        if (outputDomain && source.groupKey == outputDomain->aggregatePlan->sources[
            outputDomain->aggregatePlan->audioSource].groupKey) {
            const auto audio = std::find_if(preparedNodes.begin(), preparedNodes.end(), [&](const auto& node) {
                return node->nodeId() == outputDomain->registration.audioAggregate;
            });
            if (audio == preparedNodes.end()) return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("Selected source purge lacks its audio aggregate"));
            const auto* runtime = dynamic_cast<MediaContinuousAudioAggregateNode*>(audio->get());
            if (!runtime) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Audio aggregate role differs"));
            if (auto status = assembler.value().registerTarget(MediaAvGenerationParticipant::CanonicalLineage,
                {"aggregate_audio", runtime->sourcePurgeTarget()}); !status) return status;
        }
        auto participants = assembler.value().seal();
        if (!participants) return ::media::Status::failure(participants.error());
        std::vector<std::shared_ptr<MediaNodeWakeup>> wakeups;
        wakeups.reserve(source.members.size());
        for (const auto member : source.members) wakeups.push_back(context.sharedNodeWakeup(member));
        if (outputDomain) {
            wakeups.push_back(context.sharedNodeWakeup(outputDomain->registration.activationOwner));
            if (source.groupKey == outputDomain->aggregatePlan->sources[outputDomain->aggregatePlan->audioSource].groupKey)
                wakeups.push_back(context.sharedNodeWakeup(outputDomain->registration.audioAggregate));
        }
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
