#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/runtime/validation/MediaAvCommonCoreShapeValidator.h"
#include "internal/graph/runtime/validation/MediaAvRuntimeRegistrationValidator.h"
#include "internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.h"
#include "internal/graph/runtime/validation/MediaSourceClockShapeValidator.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"
#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"

#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isSynchronizedNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::AvContinuousAggregate:
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::ScheduledOutputRouter:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
        return true;
    default:
        return false;
    }
}

} // namespace

::media::Status MediaAvSyncGraphShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    const auto invalid = [](const char* message) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
    };
    if (binding.domains.empty() || !binding.outputGroupKey.valid())
        return invalid("Synchronized graph requires typed domains and its explicit output group");
    std::unordered_set<std::string> groups;
    std::unordered_map<std::uint64_t, std::size_t> owners;
    std::vector<std::vector<MediaNodeId>> members(binding.domains.size());
    const MediaAvOutputDomainBinding* continuousOutput = nullptr;
    std::size_t outputCount = 0;
    std::size_t sourceCount = 0;
    for (std::size_t index = 0; index < binding.domains.size(); ++index) {
        const auto& domain = binding.domains[index];
        if (!domain.groupKey.valid() || !groups.insert(domain.groupKey.value()).second)
            return invalid("A/V domains require unique valid group identities");
        if (auto plan = MediaAvSyncPlanValidator::validateRuntime(domain.plan); !plan) return plan;
        if (const auto* shared = std::get_if<MediaAvSharedSourceOutputDomainBinding>(&domain.role)) {
            if (binding.domains.size() != 1)
                return invalid("Shared source/output registration is a complete single domain contract");
            shared->registration.processing.forEach([&](MediaNodeId id) { members[index].push_back(id); });
            ++outputCount;
        } else if (const auto* source = std::get_if<MediaAvSourceDomainBinding>(&domain.role)) {
            members[index] = source->registration.processingMembers;
            ++sourceCount;
        } else {
            continuousOutput = &std::get<MediaAvOutputDomainBinding>(domain.role);
            members[index] = continuousOutput->registration.processingMembers;
            ++outputCount;
        }
        const bool producesOutput = !std::holds_alternative<MediaAvSourceDomainBinding>(domain.role);
        if (producesOutput != (domain.groupKey == binding.outputGroupKey))
            return invalid("A/V output group must identify its unique declared output domain");
        if (members[index].empty()) return invalid("A/V domain cannot own an empty processing partition");
        for (const auto member : members[index]) {
            if (!member.isValid() || !graph.findNode(member) || !owners.emplace(member.value, index).second)
                return invalid("A/V domain ownership must be valid, disjoint and duplicate-free");
        }
    }
    if (outputCount != 1 || owners.size() != graph.nodes().size())
        return invalid("A/V domains must cover the complete graph with one output authority");
    if (continuousOutput) {
        if (!sourceCount || !continuousOutput->aggregatePlan ||
            binding.audioExecutionProduct != MediaSynchronizedAudioExecutionProduct::FrameTranscode)
            return invalid("Continuous aggregation requires source domains and planned audio transcode");
        const auto& aggregate = *continuousOutput->aggregatePlan;
        if (aggregate.outputGroupKey != binding.outputGroupKey || aggregate.sources.size() != sourceCount ||
            aggregate.audioSource >= sourceCount || !continuousOutput->preparedVideoEncoder)
            return invalid("Aggregate source and output identities disagree with runtime domains");
        std::unordered_set<std::string> aggregateGroups;
        for (const auto& input : aggregate.sources) {
            bool found = false;
            for (const auto& domain : binding.domains) {
                if (domain.groupKey == input.groupKey && std::holds_alternative<MediaAvSourceDomainBinding>(domain.role)) found = true;
            }
            if (!found || !aggregateGroups.insert(input.groupKey.value()).second)
                return invalid("Aggregate inputs must name each registered source domain exactly once");
        }
    }
    for (std::size_t index = 0; index < binding.domains.size(); ++index) {
        const auto& domain = binding.domains[index];
        const MediaAvDomainValidationView view{domain.groupKey, domain.plan, domain.role,
            binding.edgePolicies, binding.datagramTransport, binding.audioExecutionProduct,
            binding.outputProduct, members[index]};
        if (!std::holds_alternative<MediaAvOutputDomainBinding>(domain.role)) {
            if (auto source = MediaSourceClockShapeValidator::validate(graph, view); !source) return source;
        }
        if (auto common = MediaAvCommonCoreShapeValidator::validate(graph, view); !common) return common;
        if (auto registration = MediaAvRuntimeRegistrationValidator::validate(graph, view); !registration) return registration;
        if (domain.groupKey == binding.outputGroupKey) {
            if (auto output = MediaOutputAuthorityShapeValidator::validate(graph, view); !output) return output;
        }
    }
    if (continuousOutput) {
        const auto& aggregate = *continuousOutput->aggregatePlan;
        const auto aggregateId = continuousOutput->registration.activationOwner;
        const auto* aggregateNode = graph.findNode(aggregateId);
        const auto incoming = [&](const char* name) -> const MediaEdge* {
            const auto* port = aggregateNode->findInputPort(name);
            return port ? MediaGraphShapeQuery::singleIncomingEdge(graph, port->id) : nullptr;
        };
        const auto discardedAudioCount = std::count_if(aggregate.sources.begin(), aggregate.sources.end(),
            [](const auto& source) { return source.discardedAudioPort.has_value(); });
        if (aggregateNode->inputPorts.size() != 2u * aggregate.sources.size() + 3u + discardedAudioCount ||
            aggregateNode->outputPorts.size() != 3u ||
            !MediaGraphShapeQuery::validPort(aggregateNode->findOutputPort("video"),
                MediaPortDirection::Output, MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame) ||
            !MediaGraphShapeQuery::validPort(aggregateNode->findOutputPort("audio"),
                MediaPortDirection::Output, MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame) ||
            !MediaGraphShapeQuery::validPort(aggregateNode->findOutputPort("activated"),
                MediaPortDirection::Output, MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent))
            return invalid("Aggregate ports must match its source frames, epochs, audio and codec contracts");
        for (const auto& codec : {std::pair{"video_codec", MediaNodeKind::CodecResolver},
                                  std::pair{"audio_codec", MediaNodeKind::AudioCodecResolver}}) {
            const auto* edge = incoming(codec.first);
            if (!edge || !graph.findNode(edge->from.nodeId) || !graph.findPort(edge->from.portId) ||
                !owners.contains(edge->from.nodeId.value) ||
                graph.findNode(edge->from.nodeId)->kind != codec.second ||
                graph.findPort(edge->from.portId)->name != "encoder" ||
                binding.domains[owners.at(edge->from.nodeId.value)].groupKey != binding.outputGroupKey)
                return invalid("Aggregate codec preparation must come from its output domain resolver");
        }
        for (std::size_t index = 0; index < aggregate.sources.size(); ++index) {
            const auto& inputPlan = aggregate.sources[index];
            const MediaAvSourceDomainBinding* source = nullptr;
            for (const auto& domain : binding.domains) {
                if (domain.groupKey == inputPlan.groupKey)
                    source = std::get_if<MediaAvSourceDomainBinding>(&domain.role);
            }
            const auto* video = incoming(inputPlan.videoPort.c_str());
            const auto epochPort = "source_epoch_" + std::to_string(index);
            const auto* epoch = incoming(epochPort.c_str());
            if (!source || !video || !epoch || !graph.findPort(video->from.portId) ||
                !graph.findPort(epoch->from.portId) ||
                video->from.nodeId != source->registration.preparationOwner ||
                graph.findPort(video->from.portId)->name != "frame" ||
                epoch->from.nodeId != source->registration.input.activationSequencer ||
                graph.findPort(epoch->from.portId)->name != "activated")
                return invalid("Aggregate inputs must bind each source preparation frame and activation event exactly");
            if (inputPlan.discardedAudioPort) {
                const auto* discarded = incoming(inputPlan.discardedAudioPort->c_str());
                if (index == aggregate.audioSource || inputPlan.discardedAudioPort->empty() ||
                    !discarded || !graph.findNode(discarded->from.nodeId) ||
                    !graph.findPort(discarded->from.portId) || !owners.contains(discarded->from.nodeId.value) ||
                    graph.findNode(discarded->from.nodeId)->kind != MediaNodeKind::AudioResample ||
                    graph.findPort(discarded->from.portId)->name != "frame" ||
                    binding.domains[owners.at(discarded->from.nodeId.value)].groupKey != inputPlan.groupKey)
                    return invalid("Discarded audio must bind its nonselected source resampler exactly");
            }
        }
        const auto* audio = incoming(aggregate.audioPort.c_str());
        if (!audio || !graph.findNode(audio->from.nodeId) || !graph.findPort(audio->from.portId) ||
            !owners.contains(audio->from.nodeId.value) || graph.findNode(audio->from.nodeId)->kind != MediaNodeKind::AudioResample ||
            graph.findPort(audio->from.portId)->name != "frame" ||
            binding.domains[owners.at(audio->from.nodeId.value)].groupKey != aggregate.sources[aggregate.audioSource].groupKey)
            return invalid("Aggregate audio must bind the planned source resampler");
        for (const auto& edge : graph.edges()) {
            if (!owners.contains(edge.from.nodeId.value) || !owners.contains(edge.to.nodeId.value))
                return invalid("A/V edge refers to an unowned node");
            const auto fromOwner = owners.at(edge.from.nodeId.value);
            const auto toOwner = owners.at(edge.to.nodeId.value);
            const auto* from = graph.findNode(edge.from.nodeId);
            const auto* to = graph.findNode(edge.to.nodeId);
            const auto* fromPort = graph.findPort(edge.from.portId);
            const auto* toPort = graph.findPort(edge.to.portId);
            if (!from || !to || !fromPort || !toPort)
                return invalid("A/V domain edge has invalid endpoints");
            if (to->kind == MediaNodeKind::VideoEncode || to->kind == MediaNodeKind::AudioEncode) {
                if (toPort->name == "frame" &&
                    (from->id != aggregateId || fromPort->name != (to->kind == MediaNodeKind::VideoEncode ? "video" : "audio")))
                    return invalid("Continuous encoder frames must come from the unique aggregate boundary");
            }
            if (fromOwner == toOwner) continue;
            const bool fromOutput = binding.domains[fromOwner].groupKey == binding.outputGroupKey;
            const bool toOutput = binding.domains[toOwner].groupKey == binding.outputGroupKey;
            if (fromOutput == toOutput)
                return invalid("A/V source domains cannot exchange processing payloads directly");
            bool allowed = false;
            if (to->id == aggregateId) {
                for (std::size_t index = 0; index < aggregate.sources.size(); ++index) {
                    if (binding.domains[fromOwner].groupKey != aggregate.sources[index].groupKey) continue;
                    const auto& source = std::get<MediaAvSourceDomainBinding>(binding.domains[fromOwner].role);
                    allowed = (toPort->name == aggregate.sources[index].videoPort &&
                        from->id == source.registration.preparationOwner && fromPort->name == "frame") ||
                        (toPort->name == "source_epoch_" + std::to_string(index) &&
                         from->id == source.registration.input.activationSequencer && fromPort->name == "activated") ||
                        (index == aggregate.audioSource && toPort->name == aggregate.audioPort &&
                         from->kind == MediaNodeKind::AudioResample && fromPort->name == "frame") ||
                        (aggregate.sources[index].discardedAudioPort &&
                         toPort->name == *aggregate.sources[index].discardedAudioPort &&
                         from->kind == MediaNodeKind::AudioResample && fromPort->name == "frame");
                }
            } else if (fromOutput && fromPort->name == "encoder" && toPort->name == "codec") {
                allowed = (from->kind == MediaNodeKind::CodecResolver && to->kind == MediaNodeKind::VideoFilter) ||
                    (from->kind == MediaNodeKind::AudioCodecResolver && to->kind == MediaNodeKind::AudioResample);
            }
            if (!allowed) return invalid("A/V cross-domain edge bypasses its planned aggregation or codec preparation boundary");
        }
    }
    const MediaAvSyncGraphShape complete(graph);
    if (auto aggregateCount = complete.requireExact({
            {MediaNodeKind::AvContinuousAggregate, continuousOutput ? 1u : 0u, "continuous aggregate"}},
            "A/V domain shape"); !aggregateCount) return aggregateCount;
    return ::media::Status::success();
}

::media::Status MediaAvSyncGraphShapeValidator::validateAbsent(
    const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (isSynchronizedNode(node.kind)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized runtime nodes require an A/V sync binding"));
        }
        for (const auto& [key, value] : node.options.values()) {
            if (key.ends_with(".sync_group")) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized runtime options require an A/V sync binding"));
            }
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
