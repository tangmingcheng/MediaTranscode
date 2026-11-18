#include "internal/graph/planner/realtime/MediaCompositionResidenceReadbackValidator.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
::media::Status MediaCompositionResidenceReadbackValidator::validate(const MediaGraph& graph,
    const MediaCompositionGraphTopology& topology,const MediaCompositionGraphIdentity& identity,
    const MediaCompositionResidenceFacts& facts,const MediaCompositionReferenceResidencePlan& reference,
    const MediaFinalGraphReferenceStoragePlan& actual)
try {
    const auto invalid=[](const char* text){return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text));};
    if(identity.positions.size()!=topology.positions.size() || identity.connections.size()!=topology.connections.size() ||
        reference.nodeReferences.size()!=identity.positions.size() || graph.nodes().size()!=identity.positions.size() ||
        graph.edges().size()!=identity.connections.size() || actual.startupNodeMedia.size()!=reference.startupNodes.size() ||
        actual.startupEventMedia.size()!=reference.startupEvents.size())return invalid("Composition residence readback membership differs");
    const auto nodeFor=[&](MediaCompositionGraphRole role)->const MediaNode*{
        for(const auto& binding:topology.bindings)if(binding.role==role && binding.position<identity.positions.size())
            return graph.findNode(identity.positions[binding.position]);
        return nullptr;
    };
    for(std::size_t source=0;source<facts.sources.size();++source){
        const auto& expected=facts.sources[source];
        const auto rtp=[&](MediaRtpClockRouteRole role){return nodeFor(compositionSourceRole(source,
            {MediaSourceTopologyDomain::Rtp,static_cast<std::size_t>(role)}));};
        const auto option=[&](MediaRtpClockRouteRole role,const char* key,std::uint64_t value){
            const auto* node=rtp(role);return node && node->options.value(key)==std::to_string(value);
        };
        if(!option(MediaRtpClockRouteRole::VideoInput,"rtp.ingress.descriptor_capacity",expected.videoDescriptorCapacity) ||
            !option(MediaRtpClockRouteRole::VideoBinder,"rtp_clock_binder.acquiring_capacity",expected.videoAcquiringCapacity))
            return invalid("Composition video holder readback differs from pure facts");
        if(expected.audioDescriptorCapacity && (!expected.audioAcquiringCapacity ||
            !option(MediaRtpClockRouteRole::AudioInput,"rtp.ingress.descriptor_capacity",*expected.audioDescriptorCapacity) ||
            !option(MediaRtpClockRouteRole::AudioBinder,"rtp_clock_binder.acquiring_capacity",*expected.audioAcquiringCapacity)))
            return invalid("Composition audio holder readback differs from pure facts");
        const auto* filter=nodeFor(compositionSourceRole(source,{MediaSourceTopologyDomain::VideoFrames,
            static_cast<std::size_t>(MediaVideoBranchRole::Filter)}));
        if((filter!=nullptr)!=expected.filterRetainedInputs.has_value())return invalid("Composition filter holder membership differs");
        if(filter){
            auto execution=MediaVideoFilterExecutionPlanCodec::decode(filter->options);
            if(!execution)return ::media::Status::failure(execution.error());
            const auto retained=execution.value().output.maximumRetainedInputFrames;
            if(retained!=*expected.filterRetainedInputs)return invalid("Composition filter input retention readback differs");
        }
    }
    for(std::size_t position=0;position<identity.positions.size();++position){
        const auto id=identity.positions[position];
        const auto* node=graph.findNode(id);
        if(!node || node->kind!=topology.positions[position].kind || std::count(identity.positions.begin(),identity.positions.end(),id)!=1)
            return invalid("Composition residence node identity differs");
        const auto expected=reference.nodeReferences[position];
        const bool aggregate=std::find(reference.aggregate.retainingPositions.begin(),reference.aggregate.retainingPositions.end(),position)!=reference.aggregate.retainingPositions.end();
        if(std::count_if(actual.entries.begin(),actual.entries.end(),[&](const auto& entry){
            return entry.referenceNode==id;
        })!=(aggregate?0:1))return invalid("Composition ordinary node readback is missing or repeated");
        if(!expected)continue;
        const auto entry=std::find_if(actual.entries.begin(),actual.entries.end(),[&](const auto& value){return value.referenceNode==id;});
        if(entry->maximumBufferObjects!=*expected)return invalid("Composition ordinary node reference readback differs");
    }
    for(std::size_t index=0;index<identity.connections.size();++index){
        const auto id=identity.connections[index];
        const auto* edge=graph.findEdge(id);
        const auto& planned=topology.connections[index];
        const auto* from=nodeFor(planned.from);
        const auto* to=nodeFor(planned.to);
        if(!edge || !from || !to || std::count(identity.connections.begin(),identity.connections.end(),id)!=1 ||
            edge->from.nodeId!=from->id || edge->to.nodeId!=to->id || edge->policy!=planned.policy ||
            edge->streamKind!=planned.stream || edge->payloadKind!=planned.payload)
            return invalid("Composition residence edge identity or policy differs");
        if(std::count_if(actual.entries.begin(),actual.entries.end(),[&](const auto& entry){return entry.referenceEdge==id;})!=1)
            return invalid("Composition ordinary edge readback is missing or repeated");
        const auto entry=std::find_if(actual.entries.begin(),actual.entries.end(),[&](const auto& value){return value.referenceEdge==id;});
        if(entry->maximumBufferObjects!=static_cast<std::uint64_t>(edge->policy.queuePolicy.capacity))
            return invalid("Composition ordinary edge references differ from queue capacity");
    }
    for(const auto& expected:reference.startupNodes){
        if(expected.position>=identity.positions.size() || expected.coordinator>=identity.positions.size())
            return invalid("Composition startup node position is invalid");
        if(std::count_if(actual.startupNodeMedia.begin(),actual.startupNodeMedia.end(),[&](const auto& bound){
            return bound.node==identity.positions[expected.position] && bound.coordinator==identity.positions[expected.coordinator] &&
                bound.videoPayload==expected.videoPayload && bound.videoUnits==expected.units.video && bound.audioPacketUnits==expected.units.audio;
        })!=1)return invalid("Composition startup node media readback differs");
    }
    for(const auto& expected:reference.startupEvents){
        if(expected.connection>=identity.connections.size() || expected.coordinator>=identity.positions.size())
            return invalid("Composition startup event position is invalid");
        if(std::count_if(actual.startupEventMedia.begin(),actual.startupEventMedia.end(),[&](const auto& bound){
            return bound.edge==identity.connections[expected.connection] && bound.coordinator==identity.positions[expected.coordinator] &&
                bound.videoPayload==expected.videoPayload && bound.videoUnits==expected.units.media.video &&
                bound.audioPacketUnits==expected.units.media.audio && bound.form==expected.units.form && bound.wrapperSlots==expected.units.wrapperSlots;
        })!=1)return invalid("Composition startup event media readback differs");
    }
    std::vector<MediaNodeId> aggregate;
    for(const auto position:reference.aggregate.retainingPositions){
        if(position>=identity.positions.size())return invalid("Composition aggregate retaining position is invalid");
        aggregate.push_back(identity.positions[position]);
    }
    std::size_t aggregateOwners=0;
    for(const auto& entry:actual.entries){
        if(!entry.validReferenceIdentity())
            return invalid("Composition reference readback has conflicting identity domains");
        if(entry.referenceNode && std::find(identity.positions.begin(),identity.positions.end(),*entry.referenceNode)==identity.positions.end())
            return invalid("Composition ordinary node owner is outside the projection");
        if(entry.referenceEdge && std::find(identity.connections.begin(),identity.connections.end(),*entry.referenceEdge)==identity.connections.end())
            return invalid("Composition ordinary edge owner is outside the projection");
        if(!entry.maximumBufferObjects || entry.retentionNodes.empty())continue;
        if(entry.retentionNodes.size()!=aggregate.size() || entry.maximumBufferObjects!=reference.aggregate.maximumReferences ||
            !std::is_permutation(entry.retentionNodes.begin(),entry.retentionNodes.end(),aggregate.begin(),aggregate.end()))
            return invalid("Composition shared reference owner differs from aggregate facts");
        ++aggregateOwners;
    }
    if(aggregateOwners!=1)return invalid("Composition aggregate shared owner is missing or repeated");
    return ::media::Status::success();
}catch(const std::bad_alloc&){return ::media::Status::failure(::media::ErrorInfo::allocationFailed("composition residence readback"));}
}
