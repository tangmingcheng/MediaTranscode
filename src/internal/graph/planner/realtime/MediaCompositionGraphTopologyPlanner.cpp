#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
std::optional<std::size_t> MediaCompositionGraphTopology::position(MediaCompositionGraphRole role) const noexcept {
    for (const auto& binding : bindings) if (binding.role == role) return binding.position;
    return std::nullopt;
}
const MediaCompositionGraphAttachment* MediaCompositionGraphTopology::attachment(MediaCompositionGraphRole role) const noexcept {
    const auto found=std::find_if(attachments.begin(),attachments.end(),[&](const auto& value){return value.role==role;});
    return found==attachments.end()?nullptr:&*found;
}
::media::Result<MediaCompositionGraphTopology> MediaCompositionGraphTopologyPlanner::plan(const MediaCompositionGraphTopologyFacts& facts)
try {
    using Result=::media::Result<MediaCompositionGraphTopology>;
    using D=MediaCompositionGraphDomain;
    using G=MediaCompositionAggregateRole;
    using V=MediaVideoBranchRole;
    using A=MediaAudioBranchRole;
    using O=MediaOutputSegmentRole;
    const auto invalid=[](const char* message){return Result::failure(::media::ErrorInfo::invalidArgument(message));};
    MediaCompositionGraphTopology result;
    const auto key=[](D domain,auto role){return compositionOutputRole(domain,role);};
    const auto declare=[&](MediaCompositionGraphRole role,MediaNodeKind kind)->bool{
        if(result.position(role) || kind==MediaNodeKind::Unknown)return false;
        result.bindings.push_back({role,result.positions.size()});
        result.positions.push_back({kind,role.source,{}});
        return true;
    };
    const auto attach=[&](MediaCompositionGraphRole role,MediaCompositionGraphRole owner,std::string port)->bool{
        const auto position=result.position(owner);
        if(!position || result.position(role) || result.attachment(owner))return false;
        result.attachments.push_back({role,owner,std::move(port)});
        result.bindings.push_back({role,*position});
        return true;
    };
    const auto branchNodes=[&](D domain,const auto& shape,auto count)->bool{
        using Role=decltype(count);
        for(std::size_t i=0;i<static_cast<std::size_t>(count);++i){
            const auto role=static_cast<Role>(i);
            if(shape.has(role))if(const auto kind=shape.ownedKind(role))if(!declare(key(domain,role),*kind))return false;
        }
        return true;
    };
    if(facts.sources.empty() || facts.aggregate.sources.size()!=facts.sources.size() ||
        !branchNodes(D::VideoOutput,facts.videoShape,V::Count) || !branchNodes(D::AudioOutput,facts.audioShape,A::Count))
        return invalid("Composition graph requires unique output declarations and source facts");
    for(const auto& node:MediaCompositionAggregateNodes)if(!declare(key(D::Aggregate,node.role),node.kind))
        return invalid("Composition aggregate node is repeated");
    for(const auto& segment:{std::pair{D::Scheduler,&facts.scheduler},std::pair{D::Protocol,&facts.protocol},std::pair{D::Execution,&facts.execution}})
        for(const auto& node:segment.second->nodes)if(!declare(key(segment.first,node.role),node.kind))
            return invalid("Composition output segment node is repeated");
    if(!attach(key(D::VideoOutput,V::OutputCodec),key(D::VideoOutput,V::CodecResolver),"encoder") ||
        !attach(key(D::AudioOutput,A::OutputCodec),key(D::AudioOutput,A::CodecResolver),"encoder") ||
        !attach(key(D::Aggregate,G::OutputVideoCodec),key(D::VideoOutput,V::CodecResolver),"encoder") ||
        !attach(key(D::Aggregate,G::OutputAudioCodec),key(D::AudioOutput,A::CodecResolver),"encoder") ||
        !attach(key(D::Aggregate,G::OutputVideoFrame),key(D::VideoOutput,V::Encode),"frame") ||
        !attach(key(D::Aggregate,G::OutputAudioFrame),key(D::AudioOutput,A::Encode),"frame") ||
        !attach(key(D::Scheduler,O::VideoPacket),key(D::VideoOutput,V::Encode),"packet") ||
        !attach(key(D::Scheduler,O::AudioPacket),key(D::AudioOutput,facts.audioShape.has(A::Canonicalizer)?A::Canonicalizer:A::Encode),
            facts.audioShape.has(A::Canonicalizer)?"canonical":"packet") ||
        !attach(key(D::Execution,O::Activation),key(D::Aggregate,G::VideoAggregate),"activated"))
        return invalid("Composition output attachment has no unique declared owner");
    // Every protocol external role is resolved explicitly. Equal role names in
    // different segments never imply equal nodes or equal ports.
    for(const auto& edge:facts.protocol.connections)for(const auto role:{edge.from,edge.to}) {
        if(result.position(key(D::Protocol,role)))continue;
        bool attached=false;
        switch(role){
        case O::Activation:attached=attach(key(D::Protocol,role),key(D::Aggregate,G::VideoAggregate),"activated");break;
        case O::VideoCodec:attached=attach(key(D::Protocol,role),key(D::VideoOutput,V::Encode),"codec");break;
        case O::AudioCodec:attached=attach(key(D::Protocol,role),key(D::AudioOutput,A::Encode),"codec");break;
        case O::VideoPacket:attached=attach(key(D::Protocol,role),key(D::Scheduler,O::Router),"video");break;
        case O::AudioPacket:attached=attach(key(D::Protocol,role),key(D::Scheduler,O::Router),"audio");break;
        case O::SerializedPacket:attached=attach(key(D::Protocol,role),key(D::Scheduler,O::Router),"serialized");break;
        case O::TransportPlan:case O::Sender:attached=attach(key(D::Protocol,role),key(D::Execution,role),"");break;
        default:break;
        }
        if(!attached)return invalid("Composition protocol external role has no explicit attachment");
    }
    for(std::size_t source=0;source<facts.sources.size();++source){
        const auto& topology=facts.sources[source];
        std::vector<std::optional<std::size_t>> positions(topology.positions.size());
        for(std::size_t position=0;position<topology.positions.size();++position){
            const auto binding=std::find_if(topology.bindings.begin(),topology.bindings.end(),[&](const auto& value){return value.position==position;});
            if(binding==topology.bindings.end())return invalid("Composition source position has no role");
            if(topology.positions[position]){
                positions[position]=result.positions.size();
                result.positions.push_back({*topology.positions[position],source,{}});
            }else{
                const auto boundary=std::find_if(topology.bindings.begin(),topology.bindings.end(),[&](const auto& value){
                    return value.position==position && value.role.domain==MediaSourceTopologyDomain::Aggregate;
                });
                if(boundary==topology.bindings.end())return invalid("Composition source external position lacks aggregate attachment");
                const auto target=key(D::Aggregate,static_cast<G>(boundary->role.role));
                positions[position]=result.position(target);
                if(!positions[position])return invalid("Composition source external position has no output owner");
                const auto* attachment=result.attachment(target);
                const auto owner=attachment?attachment->owner:target;
                const auto port=attachment?attachment->port:std::string{};
                for(const auto& alias:topology.bindings)if(alias.position==position)
                    result.attachments.push_back({compositionSourceRole(source,alias.role),owner,port});
            }
        }
        for(const auto& binding:topology.bindings){
            const auto role=compositionSourceRole(source,binding.role);
            if(binding.position>=positions.size() || !positions[binding.position] || result.position(role))
                return invalid("Composition source binding is repeated or invalid");
            result.bindings.push_back({role,*positions[binding.position]});
        }
        for(const auto& edge:topology.connections)result.connections.push_back({compositionSourceRole(source,edge.from),
            compositionSourceRole(source,edge.to),edge.output,edge.input,edge.payload,edge.stream,edge.policy});
    }
    const auto branchEdges=[&](D domain,const auto& edges){
        for(const auto& edge:edges)result.connections.push_back({key(domain,edge.from),key(domain,edge.to),
            std::string(edge.output),std::string(edge.input),edge.payload,edge.stream,edge.policy});
    };
    branchEdges(D::VideoOutput,facts.videoConnections);
    branchEdges(D::AudioOutput,facts.audioConnections);
    branchEdges(D::Aggregate,facts.aggregate.output);
    branchEdges(D::Scheduler,facts.scheduler.connections);
    branchEdges(D::Protocol,facts.protocol.connections);
    branchEdges(D::Execution,facts.execution.connections);
    // Source->aggregate edges are already in source products. Do not deduplicate
    // by label or policy: an accidental duplicate is an invalid declaration.
    for(const auto& edge:result.connections){
        const auto from=result.position(edge.from),to=result.position(edge.to);
        if(!from || !to || !edge.policy.queuePolicy.bounded || edge.policy.queuePolicy.capacity==0)
            return invalid("Composition graph edge lacks owned positions or bounded capacity");
        const auto capacity=static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity);
        result.positions[*from].incidentCapacities.push_back(capacity);
        if(*to!=*from)result.positions[*to].incidentCapacities.push_back(capacity);
    }
    return Result::success(std::move(result));
}catch(const std::bad_alloc&){return ::media::Result<MediaCompositionGraphTopology>::failure(
    ::media::ErrorInfo::allocationFailed("composition complete topology projection"));}
}
