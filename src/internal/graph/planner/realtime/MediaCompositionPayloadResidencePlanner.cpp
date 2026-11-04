#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaNodeReferenceRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
namespace {
std::optional<std::size_t> position(const MediaCompositionGraphTopology& topology,MediaCompositionGraphRole role){
    for(const auto& binding:topology.bindings)if(binding.role==role)return binding.position;
    return std::nullopt;
}
MediaCompositionGraphRole startupRole(std::size_t source,MediaStartupRouteRole role){
    return compositionSourceRole(source,{MediaSourceTopologyDomain::Startup,static_cast<std::size_t>(role)});
}
std::optional<std::uint64_t> mediaUnits(MediaStreamKind stream,MediaPayloadKind payload,
    MediaPayloadKind videoPayload,const MediaStartupRouteMediaUnits& units){
    if(stream==MediaStreamKind::Video && payload==videoPayload)return units.video;
    if(stream==MediaStreamKind::Audio && payload==MediaPayloadKind::Packet)return units.audio;
    return std::nullopt;
}
}
::media::Result<MediaCompositionReferenceResidencePlan> MediaCompositionPayloadResidencePlanner::compile(
    const MediaCompositionGraphTopology& topology,const MediaCompositionResidenceFacts& facts)
try {
    using Result=::media::Result<MediaCompositionReferenceResidencePlan>;
    const auto invalid=[](const char* text){return Result::failure(::media::ErrorInfo::invalidArgument(text));};
    if(facts.sources.empty() || !facts.videoEncoderRetainedFrames || !facts.audioEncoderRetainedFrames || !facts.aggregateReferences)
        return invalid("Composition reference residence lacks finite authoritative holders");
    MediaCompositionReferenceResidencePlan result;
    result.nodeReferences.resize(topology.positions.size());
    for(std::size_t source=0;source<facts.sources.size();++source){
        const auto& fact=facts.sources[source];
        const bool audio=fact.route.shape.members==MediaTranscodeStreamSet::AudioVideo;
        if(!fact.videoDescriptorCapacity || audio!=fact.audioAcquiringCapacity.has_value() ||
            audio!=fact.audioDescriptorCapacity.has_value() || (audio && !*fact.audioDescriptorCapacity))
            return invalid("Composition source residence lacks exact input holder facts");
        auto media=MediaStartupRouteMediaPlanner::plan(fact.route,fact.videoBatchUnits,fact.audioBatchUnits);
        if(!media)return Result::failure(media.error());
        const auto coordinator=position(topology,startupRole(source,MediaStartupRouteRole::Coordinator));
        if(!coordinator)return invalid("Composition startup coordinator position is absent");
        for(std::size_t role=0;role<media.value().nodes.size();++role){
            if(!media.value().nodes[role])continue;
            const auto owned=position(topology,startupRole(source,static_cast<MediaStartupRouteRole>(role)));
            if(!owned || *owned>=topology.positions.size())return invalid("Composition typed startup node is absent");
            result.startupNodes.push_back({*owned,*coordinator,media.value().videoPayload,*media.value().nodes[role]});
        }
        const auto& connections=fact.route.shape.connections();
        for(std::size_t route=0;route<connections.size();++route){
            if(!media.value().events[route])continue;
            const auto& expected=connections[route];
            std::optional<std::size_t> match;
            for(std::size_t edge=0;edge<topology.connections.size();++edge){
                const auto& actual=topology.connections[edge];
                if(actual.from==startupRole(source,expected.from) && actual.to==startupRole(source,expected.to) &&
                    actual.output==expected.output && actual.input==expected.input){
                    if(match || actual.policy!=fact.route.policy(expected) || actual.payload!=MediaPayloadKind::GraphEvent ||
                        actual.stream!=MediaStreamKind::Metadata)return invalid("Composition typed startup edge differs from route");
                    match=edge;
                }
            }
            if(!match)return invalid("Composition typed startup edge is absent");
            result.startupEvents.push_back({*match,*coordinator,media.value().videoPayload,*media.value().events[route]});
        }
    }
    for(const auto role:{MediaCompositionAggregateRole::VideoAggregate,MediaCompositionAggregateRole::AudioAggregate}){
        const auto owned=position(topology,compositionOutputRole(MediaCompositionGraphDomain::Aggregate,role));
        if(!owned)return invalid("Composition aggregate retaining position is absent");
        result.aggregate.retainingPositions.push_back(*owned);
    }
    result.aggregate.maximumReferences=facts.aggregateReferences;
    result.aggregate.kind=MediaPayloadResidenceOwnerKind::Shared;
    result.aggregate.connection=std::nullopt;
    for(std::size_t index=0;index<topology.positions.size();++index){
        const auto& owned=topology.positions[index];
        if(std::find(result.aggregate.retainingPositions.begin(),result.aggregate.retainingPositions.end(),index)!=result.aggregate.retainingPositions.end() ||
            std::any_of(result.startupNodes.begin(),result.startupNodes.end(),[&](const auto& node){return node.position==index;}))continue;
        std::optional<std::uint64_t> acquiring,descriptors,filter;
        if(owned.source){
            if(*owned.source>=facts.sources.size())return invalid("Composition residence source is absent");
            const auto& source=facts.sources[*owned.source];
            const auto rtp=[&](MediaRtpClockRouteRole role){return position(topology,compositionSourceRole(*owned.source,
                {MediaSourceTopologyDomain::Rtp,static_cast<std::size_t>(role)}));};
            if(owned.kind==MediaNodeKind::RawRtpInput){
                if(rtp(MediaRtpClockRouteRole::VideoInput)==index)descriptors=source.videoDescriptorCapacity;
                else if(rtp(MediaRtpClockRouteRole::AudioInput)==index)descriptors=source.audioDescriptorCapacity;
                else return invalid("Composition raw residence lacks scoped stream identity");
            }
            if(owned.kind==MediaNodeKind::RtpPacketClockBinder){
                if(rtp(MediaRtpClockRouteRole::VideoBinder)==index)acquiring=source.videoAcquiringCapacity;
                else if(rtp(MediaRtpClockRouteRole::AudioBinder)==index)acquiring=source.audioAcquiringCapacity;
                else return invalid("Composition acquiring residence lacks scoped stream identity");
            }
            if(owned.kind==MediaNodeKind::VideoFilter)filter=source.filterRetainedInputs;
        }
        auto retained=MediaNodeReferenceRetentionPlanner::plan({owned.kind,owned.incidentCapacities,acquiring,descriptors,filter});
        if(!retained)return Result::failure(retained.error());
        if(owned.kind==MediaNodeKind::VideoEncode || owned.kind==MediaNodeKind::AudioEncode){
            retained=MediaCheckedArithmetic::add(retained.value(),owned.kind==MediaNodeKind::VideoEncode?
                facts.videoEncoderRetainedFrames:facts.audioEncoderRetainedFrames,"composition prepared encoder reference retention");
            if(!retained)return Result::failure(retained.error());
        }
        result.nodeReferences[index]=retained.value();
    }
    return Result::success(std::move(result));
}catch(const std::bad_alloc&){return ::media::Result<MediaCompositionReferenceResidencePlan>::failure(
    ::media::ErrorInfo::allocationFailed("composition pure reference residence"));}
::media::Result<MediaPayloadResidenceProjection> MediaCompositionPayloadResidencePlanner::plan(
    const MediaCompositionGraphTopology& topology,const MediaCompositionReferenceResidencePlan& reference,
    std::size_t producerPosition,MediaStreamKind stream,MediaPayloadKind payload)
try {
    using Result=::media::Result<MediaPayloadResidenceProjection>;
    if(reference.nodeReferences.size()!=topology.positions.size() ||
        (stream!=MediaStreamKind::Video && stream!=MediaStreamKind::Audio) ||
        (payload!=MediaPayloadKind::Packet && payload!=MediaPayloadKind::Frame))
        return Result::failure(::media::ErrorInfo::invalidArgument("Composition residence domain is invalid"));
    MediaPayloadResidenceTopology projected{topology.positions.size(),{}, {reference.aggregate}};
    for(std::size_t index=0;index<topology.connections.size();++index){
        const auto& edge=topology.connections[index];
        const auto typed=std::find_if(reference.startupEvents.begin(),reference.startupEvents.end(),[&](const auto& value){return value.connection==index;});
        const auto units=typed==reference.startupEvents.end()?std::optional<std::uint64_t>{}:
            mediaUnits(stream,payload,typed->videoPayload,typed->units.media);
        const bool carries=typed==reference.startupEvents.end()?(edge.stream==stream && edge.payload==payload):units.has_value();
        if(!carries)continue;
        const auto from=position(topology,edge.from),to=position(topology,edge.to);
        if(!from || !to)return Result::failure(::media::ErrorInfo::invalidArgument("Composition residence edge lacks positions"));
        projected.connections.push_back({*from,*to});
        auto objects=typed==reference.startupEvents.end()?::media::Result<std::uint64_t>::success(static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity)):
            MediaCheckedArithmetic::multiply(typed->units.wrapperSlots,*units,"composition startup wrapper media references");
        if(!objects)return Result::failure(objects.error());
        projected.owners.push_back({{*from},objects.value(),typed==reference.startupEvents.end()?
            MediaPayloadResidenceOwnerKind::Edge:MediaPayloadResidenceOwnerKind::StartupEdge,index});
    }
    // Ordinary node references apply to every reachable media domain. Typed
    // nodes replace them completely, even in domains with no typed media units.
    for(std::size_t index=0;index<reference.nodeReferences.size();++index)
        if(reference.nodeReferences[index])projected.owners.push_back({{index},*reference.nodeReferences[index],
            MediaPayloadResidenceOwnerKind::Node,std::nullopt});
    for(const auto& node:reference.startupNodes)
        if(const auto units=mediaUnits(stream,payload,node.videoPayload,node.units))projected.owners.push_back({{node.position},*units,
            MediaPayloadResidenceOwnerKind::StartupNode,std::nullopt});
    return MediaGraphPayloadResidencePlanner::plan(projected,producerPosition);
}catch(const std::bad_alloc&){return ::media::Result<MediaPayloadResidenceProjection>::failure(
    ::media::ErrorInfo::allocationFailed("composition pure payload residence"));}
}
