#include "internal/graph/planner/realtime/MediaRealtimeCompositionResourcePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionResidenceReadbackValidator.h"
#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedPacketWrapperStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedFrameWrapperStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmissionPlanner.h"
#include "internal/graph/planner/realtime/MediaAvAggregateFrameCreditPlanner.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerContract.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadResidencePlanner.h"
#include "internal/graph/planner/audio/MediaAudioEncoderFifoRetentionPlan.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {
namespace {
using Arithmetic = MediaCheckedArithmetic;
::media::Result<std::uint64_t> residenceUnits(std::uint64_t numerator, std::uint64_t denominator,
    const MediaRealtimeDeploymentLatencyBudget& latency)
{
    if (!numerator || !denominator || latency.maximumResidence.nanoseconds() <= 0 || latency.authority.empty())
        return ::media::Result<std::uint64_t>::failure(::media::ErrorInfo::invalidArgument(
            "Composition allocation budget lacks authoritative cadence and residence"));
    auto scaled = Arithmetic::multiply(denominator, 1'000'000'000ULL, "composition cadence denominator");
    if (!scaled) return scaled;
    auto units = Arithmetic::ceilScale(static_cast<std::uint64_t>(latency.maximumResidence.nanoseconds()),
        numerator, scaled.value(), "composition owner residence units");
    return units ? Arithmetic::add(units.value(), 1, "composition boundary access unit") : units;
}
}

::media::Result<MediaFinalGraphResourceAdmissionPlan> MediaRealtimeCompositionResourcePlanner::plan(
    const MediaGraph& graph, const MediaCompositionGraphTopology& projection,
    const MediaCompositionGraphIdentity& identity, std::span<const MediaRealtimeCompositionSourceResources> sources,
    MediaNodeId aggregateNode, MediaNodeId audioAggregate, const MediaAvContinuousAggregateTopology& aggregate,
    const MediaVideoCanvasStorage& canvas, const MediaVideoCanvasRetentionPlan& retention,
    const MediaPreparedRealtimeEmissionSet& output, const MediaAudioEncoderFifoRetentionPlan& outputFifo,
    const MediaRealtimeDeploymentLatencyBudget& latency)
try {
    using Result = ::media::Result<MediaFinalGraphResourceAdmissionPlan>;
    const auto invalid = [](const char* text) { return Result::failure(::media::ErrorInfo::invalidArgument(text)); };
    if (sources.size() != aggregate.sources.size() || sources.empty() || !output.audio ||
        !output.video.maximumEncoderRetainedFrames || !output.audioFrames || !output.audioFrames->valid() ||
        !graph.findNode(aggregateNode) || graph.findNode(aggregateNode)->kind != MediaNodeKind::AvContinuousAggregate ||
        !graph.findNode(audioAggregate) || graph.findNode(audioAggregate)->kind != MediaNodeKind::ContinuousAudioAggregate ||
        !aggregate.maximumMetadataBytes || !aggregate.storage.maximumRetainedObjects ||
        canvas.maximumHeaderCount != retention.maximumPublishedLeases)
        return invalid("Composition admission requires all source, aggregate and prepared output products");
    if (std::count_if(graph.nodes().begin(), graph.nodes().end(), [](const auto& node) {
            return node.kind == MediaNodeKind::VideoEncode;
        }) != 1 || std::count_if(graph.nodes().begin(), graph.nodes().end(), [](const auto& node) {
            return node.kind == MediaNodeKind::AudioEncode;
        }) != 1)
        return invalid("Composition admission requires exactly one output video encoder and audio encoder");
    auto expectedFifo = MediaAudioEncoderFifoRetentionPlan::create(aggregate.audio, aggregate.audio.codecFrameSamples());
    if (!expectedFifo) return Result::failure(expectedFifo.error());
    if (expectedFifo.value() != outputFifo)
        return invalid("Output FIFO retention differs from the aggregate exact codec-frame output contract");
    auto audioUnits = residenceUnits(output.audio->accessUnitsPerSecondNumerator,
        output.audio->accessUnitsPerSecondDenominator, latency);
    if (!audioUnits) return Result::failure(audioUnits.error());
    MediaFinalGraphResourceAdmissionPlan result{
        MediaRealtimeGraphResourceBudgetScope::EngineManagedPayloadAndReservedStorage,
        0, output.video.maximumEncoderRetainedFrames, audioUnits.value(), output.hardwareMemory,
        true, retention.encoderPool, {}, {}, {}, {}};
    for (const auto& source : sources) {
        if (source.preparationStorage.empty())
            return invalid("Composition source has no preparation allocation owners");
        for (std::size_t allocation = 0; allocation < source.preparationStorage.size(); ++allocation) {
            auto preparedStorage = MediaPreparedRtpStorageAdmissionPlanner::plan(
                graph, source.preparationStorage[allocation], source.preparationOwners, allocation);
            if (!preparedStorage) return Result::failure(preparedStorage.error());
            result.fixedStorage.push_back(std::move(preparedStorage).value());
        }
    }
    for (const auto& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::AudioEncode) {
            auto bytes = Arithmetic::add(static_cast<std::uint64_t>(outputFifo.maximumBytes),
                output.audioFrames->encoderFrameBytes, "audio encoder FIFO and pending send frame logical payload");
            if (!bytes) return Result::failure(bytes.error());
            result.fixedStorage.push_back({"audio-fifo:" + node.name, {},
                MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
                bytes.value(), 0, 0, 1, false,
                "prepared-audio-encoder-FIFO-samples+single-pending-send-frame", {node.id}, {}, {}, {}, MediaFinalGraphReferenceKind::RetainingGroup});
        }
    }
    result.fixedStorage.push_back({"node:" + graph.findNode(aggregateNode)->name, {},
        MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
        aggregate.maximumMetadataBytes, 0, aggregate.storage.maximumRetainedObjects,
        aggregate.storage.maximumRetainedObjects, false, "typed-aggregate-fixed-storage+retained-immutable-metadata",
        {aggregateNode, audioAggregate}, {}, {}, {}, MediaFinalGraphReferenceKind::RetainingGroup});
    // Compile retention facts before allocation quotas. This phase neither
    // spends a payload budget nor requires a placeholder budget.
    auto storage = MediaFinalGraphResourceLedgerCompiler::compileReferenceStorage(graph, result);
    if (!storage) return Result::failure(storage.error());
    std::vector<MediaCompositionSourceResidenceFacts> sourceReferenceFacts;
    for(std::size_t index=0;index<sources.size();++index){
        if(sources[index].sourceIndex!=index || !sources[index].referenceFacts)
            return invalid("Composition pure reference facts lack exact source membership");
        sourceReferenceFacts.push_back(*sources[index].referenceFacts);
    }
    const MediaCompositionResidenceFacts residenceFacts{sourceReferenceFacts,output.video.maximumEncoderRetainedFrames,
        audioUnits.value(),aggregate.storage.maximumRetainedObjects};
    auto reference=MediaCompositionPayloadResidencePlanner::compile(projection,residenceFacts);
    if(!reference)return Result::failure(reference.error());
    if(auto status=MediaCompositionResidenceReadbackValidator::validate(graph,projection,identity,residenceFacts,
            reference.value(),storage.value());!status)return Result::failure(status.error());

    const auto addOwner = [&](MediaGraphPayloadProducerFact fact, std::uint64_t objects,
                              std::uint64_t minimumProgress, std::string authority) -> ::media::Status {
        if (!objects || fact.authority.empty()) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Composition allocation owner lacks a finite residence product"));
        const bool device = fact.frameCredit && fact.frameCredit->allocationScope ==
            MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly;
        auto bytes = Arithmetic::multiply(device ? 0 : fact.maximumLogicalBytes, objects,
            "independent composition allocation owner budget");
        if (!bytes) return ::media::Status::failure(bytes.error());
        auto total = Arithmetic::add(result.maximumGraphPayloadAndReservedStorageBytes,
            bytes.value(), "composition independent owner budgets");
        if (!total) return ::media::Status::failure(total.error());
        const auto account = "producer:" + std::to_string(fact.nodeId.value) + ":" +
            std::to_string(static_cast<unsigned>(fact.streamKind)) + ":" +
            std::to_string(static_cast<unsigned>(fact.payloadKind));
        result.maximumGraphPayloadAndReservedStorageBytes = total.value();
        result.accounts.push_back({account, bytes.value(), objects, std::move(authority)});
        result.producerAccounts.push_back(account);
        result.minimumProgressObjects.push_back(minimumProgress);
        result.producers.push_back(std::move(fact));
        return ::media::Status::success();
    };
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const auto& source = sources[index];
        if (source.sourceIndex != index || source.producers.size() != source.residence.size())
            return invalid("Composition source owner facts do not cover their source exactly");
        std::optional<std::uint64_t> decoderReferences;
        std::optional<std::uint64_t> filterReferences;
        for (const auto& producer : source.producers) {
            const auto residence = std::find_if(source.residence.begin(), source.residence.end(), [&](const auto& value) {
                return value.key == MediaGraphPayloadProducerKey{producer.nodeId, producer.streamKind, producer.payloadKind};
            });
            if (residence == source.residence.end() || residence->authority.empty() ||
                !residence->minimumProgressObjects)
                return invalid("Composition producer lacks an authoritative retention and progress contract");
            const auto actualPosition=std::find(identity.positions.begin(),identity.positions.end(),producer.nodeId);
            if(actualPosition==identity.positions.end())return invalid("Composition producer has no verified topology position");
            const auto producerPosition=static_cast<std::size_t>(actualPosition-identity.positions.begin());
            auto reachable=MediaCompositionPayloadResidencePlanner::plan(projection,reference.value(),producerPosition,
                producer.streamKind,producer.payloadKind);
            if(!reachable)return Result::failure(reachable.error());
            auto readback=MediaGraphPayloadResidencePlanner::plan(graph,residence->key,
                {storage.value().entries,storage.value().startupEventMedia,storage.value().startupNodeMedia});
            if(!readback)return Result::failure(readback.error());
            if(readback.value().maximumRetainedObjects!=reachable.value().maximumRetainedObjects ||
                readback.value().reachableNodes.size()!=reachable.value().reachablePositions.size())
                return invalid("Composition producer residence differs from actual graph readback");
            for(const auto position:reachable.value().reachablePositions)
                if(position>=identity.positions.size() || std::count(readback.value().reachableNodes.begin(),
                        readback.value().reachableNodes.end(),identity.positions[position])!=1)
                    return invalid("Composition producer reachable identity set differs from actual graph");
            if(readback.value().contributions.size()!=reachable.value().contributions.size())
                return invalid("Composition producer residence owner membership differs from actual graph");
            for(const auto& contribution:reachable.value().contributions){
                if(contribution.connection && *contribution.connection>=identity.connections.size())
                    return invalid("Composition residence owner connection is absent");
                std::vector<MediaNodeId> retainingNodes;
                for(const auto position:contribution.retainingPositions){
                    if(position>=identity.positions.size())
                        return invalid("Composition residence owner node is absent");
                    retainingNodes.push_back(identity.positions[position]);
                }
                const auto edge=contribution.connection?
                    std::optional<MediaEdgeId>{identity.connections[*contribution.connection]}:std::nullopt;
                if(std::count_if(readback.value().contributions.begin(),readback.value().contributions.end(),
                    [&](const auto& actual){
                        return actual.kind==contribution.kind && actual.edge==edge &&
                            actual.maximumReferences==contribution.maximumReferences &&
                            std::is_permutation(actual.retainingNodes.begin(),actual.retainingNodes.end(),
                                retainingNodes.begin(),retainingNodes.end());
                    })!=1)
                    return invalid("Composition producer residence owner contribution differs from actual graph");
            }
            auto objects = Arithmetic::add(reachable.value().maximumRetainedObjects,
                residence->additionalRetainedObjects, "source graph retention and codec/atomic owners");
            if (!objects) return Result::failure(objects.error());
            const auto maximumObjects = objects.value();
            if (source.frameOwnership && producer.streamKind == MediaStreamKind::Video &&
                producer.payloadKind == MediaPayloadKind::Frame) {
                if (producer.nodeId == source.frameOwnership->decoder) decoderReferences = maximumObjects;
                if (source.frameOwnership->filter && producer.nodeId == *source.frameOwnership->filter)
                    filterReferences = maximumObjects;
            }
            if (maximumObjects < residence->minimumProgressObjects)
                return invalid("Composition retention cannot accommodate its required progress owners");
            if (auto status = addOwner(producer, maximumObjects, residence->minimumProgressObjects,
                    "final-graph-payload-residence+" + residence->authority); !status)
                return Result::failure(status.error());
        }
        if (source.frameOwnership) {
            const auto& facts = *source.frameOwnership;
            if (!decoderReferences || filterReferences.has_value() != facts.filter.has_value() ||
                facts.filter.has_value() != facts.filterGeometry.has_value() || !facts.allocationOwner ||
                !source.sourceLineageRegistryCapacity || !source.sourceTransferDirection || !source.preparedPacketContainers || !source.packetStorageHolders ||
                source.preparedPacketContainers->audio.has_value() != source.packetStorageHolders->audio.has_value())
                return invalid("Source ownership evidence lacks actual producer residence or allocation owner");
            auto ownership = MediaSourceFrameOwnershipStoragePlanner::plan(facts.decoderGeometry,
                facts.filterGeometry, *decoderReferences, filterReferences);
            if (!ownership) return Result::failure(ownership.error());
            auto lineage = MediaSourceLineageStoragePlanner::plan(projection, reference.value(),
                source.sourceIndex, *source.sourceLineageRegistryCapacity, source.preparedPacketContainers->video.prefixObjects,
                *source.sourceTransferDirection);
            if (!lineage) return Result::failure(lineage.error());
            auto frameWrappers = MediaPreparedFrameWrapperStoragePlanner::plan(projection, lineage.value(),
                facts.decoderReadiness, facts.filterReadiness);
            if (!frameWrappers) return Result::failure(frameWrappers.error());
            auto projectedOwnership = MediaSourceFrameOwnershipStoragePlanner::plan(
                frameWrappers.value(), *source.packetStorageHolders,
                facts.decoderGeometry, facts.filterGeometry);
            if (!projectedOwnership) return Result::failure(projectedOwnership.error());
            if (projectedOwnership.value() != ownership.value())
                return invalid("Source frame ownership differs from final producer residence");
            std::vector<std::shared_ptr<const MediaPreparedPacketWrapperStoragePlan>> packetWrappers;
            std::uint64_t packetBytes = 0;
            const auto planPackets = [&](MediaStreamKind stream, const MediaPreparedRtpContainerBounds& containers,
                                         const MediaCompositionStreamHolderFacts& holders) -> ::media::Status {
                auto planned = MediaPreparedPacketWrapperStoragePlanner::plan(projection, reference.value(), source.sourceIndex,
                    stream, containers, holders.maximumAtomicInputCompletion);
                if (!planned) return ::media::Status::failure(planned.error());
                auto bytes = Arithmetic::add(packetBytes, planned.value().preparationBytes, "source raw packet wrapper components");
                if (!bytes) return ::media::Status::failure(bytes.error());
                packetBytes = bytes.value();
                packetWrappers.push_back(std::make_shared<const MediaPreparedPacketWrapperStoragePlan>(std::move(planned).value()));
                return ::media::Status::success();
            };
            if (auto status = planPackets(MediaStreamKind::Video, source.preparedPacketContainers->video,
                    source.packetStorageHolders->video); !status) return Result::failure(status.error());
            if (source.preparedPacketContainers->audio)
                if (auto status = planPackets(MediaStreamKind::Audio, *source.preparedPacketContainers->audio,
                        *source.packetStorageHolders->audio); !status) return Result::failure(status.error());
            const auto reservation = std::find_if(source.preparationStorage.begin(), source.preparationStorage.end(),
                [&](const auto& value) { return value.allocationOwner == facts.allocationOwner; });
            if (reservation == source.preparationStorage.end())
                return invalid("Source frame ownership does not belong to a retained preparation allocation");
            auto componentBytes = Arithmetic::add(ownership.value().bytes, lineage.value().preparationBytes,
                "source frame ownership and lineage components");
            if (!componentBytes) return Result::failure(componentBytes.error());
            componentBytes = Arithmetic::add(componentBytes.value(), packetBytes, "source preparation object components");
            if (!componentBytes) return Result::failure(componentBytes.error());
            componentBytes = Arithmetic::add(componentBytes.value(), frameWrappers.value().preparationBytes, "source raw frame wrapper component");
            if (!componentBytes) return Result::failure(componentBytes.error());
            auto ownerBytes = componentBytes.value();
            for (const auto& previous : result.sourceFrameOwnership) {
                if (previous.allocationOwner != facts.allocationOwner) continue;
                auto total = Arithmetic::add(ownerBytes, previous.storage.bytes, "shared preparation frame ownership");
                if (!total) return Result::failure(total.error());
                ownerBytes = total.value();
                if (!previous.frameWrappers) return invalid("Source preparation lost frame wrapper evidence");
                total = Arithmetic::add(ownerBytes, previous.frameWrappers->preparationBytes, "shared preparation frame wrappers");
                if (!total) return Result::failure(total.error());
                ownerBytes = total.value();
                for (const auto& packets : previous.packetWrappers) {
                    if (!packets) return invalid("Source preparation lost packet wrapper evidence");
                    total = Arithmetic::add(ownerBytes, packets->preparationBytes, "shared preparation packet wrappers");
                    if (!total) return Result::failure(total.error());
                    ownerBytes = total.value();
                }
                if (!previous.lineageStorage)
                    return invalid("Source preparation component lost lineage storage evidence");
                total = Arithmetic::add(ownerBytes, previous.lineageStorage->preparationBytes,
                    "shared preparation lineage storage");
                if (!total) return Result::failure(total.error());
                ownerBytes = total.value();
            }
            // The complete preparation reservation is already in fixedStorage.
            // Check this component without charging it twice or pretending that
            // it includes payloads, route storage, or the other prepared owners.
            if (ownerBytes > reservation->maximumAllocationBytes)
                return invalid("Frame ownership component exceeds its shared preparation reservation");
            result.sourceFrameOwnership.push_back({source.sourceIndex, facts.decoder, facts.filter,
                facts.allocationOwner, std::move(ownership).value(),
                std::make_shared<const MediaSourceLineageStoragePlan>(std::move(lineage).value()), std::move(packetWrappers),
                std::make_shared<const MediaPreparedFrameWrapperStoragePlan>(std::move(frameWrappers).value())});
        }
    }
    for (const auto& node : graph.nodes()) {
        if (!mediaGraphPayloadProducerOutput(node.kind) ||
            std::any_of(result.producers.begin(), result.producers.end(), [&](const auto& fact) { return fact.nodeId == node.id; }))
            continue;
        if (node.id == aggregateNode || node.id == audioAggregate) {
            const auto stream = node.id == aggregateNode ? MediaStreamKind::Video : MediaStreamKind::Audio;
            {
                auto credit = MediaAvAggregateFrameCreditPlanner::decode(node.options, stream);
                if (!credit) return Result::failure(credit.error());
                auto objects = stream == MediaStreamKind::Video
                    ? ::media::Result<std::uint64_t>::success(retention.maximumPublishedLeases)
                    : residenceUnits(aggregate.audio.sampleRate(), aggregate.audio.codecFrameSamples(), latency);
                if (!objects) return Result::failure(objects.error());
                // Each aggregate owner has one pending frame; AudioEncodeNode can hold
                // one incoming frame while synchronously copying into its FIFO.
                constexpr std::uint64_t aggregatePendingFrame = 1;
                constexpr std::uint64_t audioConsumerInputFrame = 1;
                if (auto status = addOwner({node.id, stream, MediaPayloadKind::Frame,
                        credit.value().maximumLogicalBytes, credit.value(), credit.value().authority},
                        objects.value(), stream == MediaStreamKind::Video ? retention.maximumPublishedLeases
                            : aggregatePendingFrame + audioConsumerInputFrame,
                        stream == MediaStreamKind::Video ? retention.encoderPool.authority
                            : "prepared-output-audio-cadence+" + latency.authority); !status)
                    return Result::failure(status.error());
            }
        } else if (node.kind == MediaNodeKind::VideoEncode || node.kind == MediaNodeKind::AudioEncode) {
            const bool video = node.kind == MediaNodeKind::VideoEncode;
            auto objects = video ? residenceUnits(output.video.accessUnitsPerSecondNumerator,
                output.video.accessUnitsPerSecondDenominator, latency) : audioUnits;
            if (!objects) return Result::failure(objects.error());
            // receive_packet reserves exactly one output packet before calling
            // FFmpeg; downstream draining does not require another packet owner.
            constexpr std::uint64_t receivePacketReservation = 1;
            if (auto status = addOwner({node.id, video ? MediaStreamKind::Video : MediaStreamKind::Audio,
                    MediaPayloadKind::Packet, video ? output.video.maximumAccessUnitPayloadBytes
                        : output.audio->maximumAccessUnitPayloadBytes, std::nullopt,
                    video ? output.video.authority : output.audio->authority}, objects.value(), receivePacketReservation,
                    "unique-prepared-output-emission+" + latency.authority); !status)
                return Result::failure(status.error());
        } else return invalid("Composition output has a producer without authoritative allocation facts");
    }
    auto total = Arithmetic::add(result.maximumGraphPayloadAndReservedStorageBytes,
        storage.value().reservedStorageBytes, "composition owner budgets and fixed storage");
    if (!total) return Result::failure(total.error());
    result.maximumGraphPayloadAndReservedStorageBytes = total.value();
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaFinalGraphResourceAdmissionPlan>::failure(
        ::media::ErrorInfo::allocationFailed("composition allocation budgets"));
}
} // namespace media::ffmpeg::graph
