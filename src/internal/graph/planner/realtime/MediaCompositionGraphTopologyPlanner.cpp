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
namespace {
template<bool Updating> ::media::Status compileTopology(
    const MediaCompositionGraphTopologyFacts& facts, MediaCompositionGraphTopology& result)
{
    using Result=::media::Status;
    using D=MediaCompositionGraphDomain;
    using G=MediaCompositionAggregateRole;
    using V=MediaVideoBranchRole;
    using A=MediaAudioBranchRole;
    using O=MediaOutputSegmentRole;
    const auto invalid=[](const char* message){return Result::failure(::media::ErrorInfo::invalidArgument(message));};
    std::size_t positionCursor=0, bindingCursor=0, attachmentCursor=0;
    const auto key=[](D domain,auto role){return compositionOutputRole(domain,role);};
    const auto locate=[&](MediaCompositionGraphRole role)->std::optional<std::size_t>{
        for(std::size_t i=0;i<bindingCursor && i<result.bindings.size();++i)
            if(result.bindings[i].role==role)return result.bindings[i].position;
        return std::nullopt;
    };
    const auto attachment=[&](MediaCompositionGraphRole role)->const MediaCompositionGraphAttachment*{
        for(std::size_t i=0;i<attachmentCursor && i<result.attachments.size();++i)
            if(result.attachments[i].role==role)return &result.attachments[i];
        return nullptr;
    };
    const auto bind=[&](MediaCompositionGraphRole role,std::size_t position)->bool{
        if(locate(role))return false;
        if constexpr(Updating){
            if(bindingCursor>=result.bindings.size() || result.bindings[bindingCursor].role!=role ||
                result.bindings[bindingCursor].position!=position)return false;
        }else result.bindings.push_back({role,position});
        ++bindingCursor;
        return true;
    };
    const auto node=[&](MediaNodeKind kind,std::optional<std::size_t> source)->bool{
        if(kind==MediaNodeKind::Unknown)return false;
        if constexpr(Updating){
            if(positionCursor>=result.positions.size() || result.positions[positionCursor].kind!=kind ||
                result.positions[positionCursor].source!=source)return false;
        }else result.positions.push_back({kind,source,std::pmr::vector<std::uint64_t>{result.resource().get()}});
        ++positionCursor;
        return true;
    };
    const auto recordAttachment=[&](MediaCompositionGraphRole role,MediaCompositionGraphRole owner,std::string_view port)->bool{
        if constexpr(Updating){
            if(attachmentCursor>=result.attachments.size())return false;
            const auto& old=result.attachments[attachmentCursor];
            if(old.role!=role || old.owner!=owner || old.port!=port)return false;
        }else result.attachments.push_back({role,owner,std::pmr::string(port,result.resource().get())});
        ++attachmentCursor;
        return true;
    };
    const auto declare=[&](MediaCompositionGraphRole role,MediaNodeKind kind)->bool{
        return bind(role,positionCursor) && node(kind,role.source);
    };
    const auto attach=[&](MediaCompositionGraphRole role,MediaCompositionGraphRole owner,std::string_view port)->bool{
        const auto position=locate(owner);
        if(!position || locate(role) || attachment(owner))return false;
        return recordAttachment(role,owner,port) && bind(role,*position);
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
        if(locate(key(D::Protocol,role)))continue;
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
        const auto sourceBase=positionCursor;
        const auto sourcePosition=[&](std::size_t position)->std::optional<std::size_t>{
            if(position>=topology.positions.size())return std::nullopt;
            if(topology.positions[position]){
                std::size_t mapped=sourceBase;
                for(std::size_t i=0;i<position;++i)if(topology.positions[i])++mapped;
                return mapped;
            }
            const auto boundary=std::find_if(topology.bindings.begin(),topology.bindings.end(),[&](const auto& value){
                return value.position==position && value.role.domain==MediaSourceTopologyDomain::Aggregate;
            });
            if(boundary==topology.bindings.end())return std::nullopt;
            return locate(key(D::Aggregate,static_cast<G>(boundary->role.role)));
        };
        for(std::size_t position=0;position<topology.positions.size();++position){
            const auto binding=std::find_if(topology.bindings.begin(),topology.bindings.end(),[&](const auto& value){return value.position==position;});
            if(binding==topology.bindings.end())return invalid("Composition source position has no role");
            if(topology.positions[position]){
                if(!node(*topology.positions[position],source))return invalid("Composition source node identity changed");
            }else{
                const auto boundary=std::find_if(topology.bindings.begin(),topology.bindings.end(),[&](const auto& value){
                    return value.position==position && value.role.domain==MediaSourceTopologyDomain::Aggregate;
                });
                if(boundary==topology.bindings.end())return invalid("Composition source external position lacks aggregate attachment");
                const auto target=key(D::Aggregate,static_cast<G>(boundary->role.role));
                if(!locate(target))return invalid("Composition source external position has no output owner");
                const auto* attached=attachment(target);
                const auto owner=attached?attached->owner:target;
                // In build mode append may reallocate attachments; reacquire the
                // owner port for every insertion rather than retaining a view.
                for(const auto& alias:topology.bindings)if(alias.position==position){
                    const auto* current=attachment(target);
                    const std::string_view currentPort=current?std::string_view(current->port):std::string_view{};
                    if(!recordAttachment(compositionSourceRole(source,alias.role),owner,currentPort))
                        return invalid("Composition source attachment identity changed");
                }
            }
        }
        for(const auto& binding:topology.bindings){
            const auto position=sourcePosition(binding.position);
            if(!position || !bind(compositionSourceRole(source,binding.role),*position))
                return invalid("Composition source binding is repeated or invalid");
        }
    }
    if(positionCursor!=result.positions.size() || bindingCursor!=result.bindings.size() ||
        attachmentCursor!=result.attachments.size())return invalid("Composition graph declaration catalog changed");
    std::size_t edgeCursor=0;
    bool edgeIdentityValid=true;
    const auto add=[&](MediaCompositionGraphRole from,MediaCompositionGraphRole to,std::string_view output,
        std::string_view input,MediaPayloadKind payload,MediaStreamKind stream,const MediaEdgePolicy& policy){
        if(!edgeIdentityValid)return;
        if constexpr(Updating){
            if(edgeCursor>=result.connections.size()){edgeIdentityValid=false;return;}
            auto& edge=result.connections[edgeCursor];
            if(edge.from!=from || edge.to!=to || edge.output!=output || edge.input!=input ||
                edge.payload!=payload || edge.stream!=stream){edgeIdentityValid=false;return;}
            edge.policy=policy;
        }else result.connections.push_back({from,to,std::pmr::string(output,result.resource().get()),std::pmr::string(input,result.resource().get()),payload,stream,policy});
        ++edgeCursor;
    };
    for(std::size_t source=0;source<facts.sources.size();++source)
        for(const auto& edge:facts.sources[source].connections)add(compositionSourceRole(source,edge.from),
            compositionSourceRole(source,edge.to),edge.output,edge.input,edge.payload,edge.stream,edge.policy);
    const auto branchEdges=[&](D domain,const auto& edges){
        for(const auto& edge:edges)add(key(domain,edge.from),key(domain,edge.to),
            edge.output,edge.input,edge.payload,edge.stream,edge.policy);
    };
    branchEdges(D::VideoOutput,facts.videoConnections);
    branchEdges(D::AudioOutput,facts.audioConnections);
    branchEdges(D::Aggregate,facts.aggregate.output);
    branchEdges(D::Scheduler,facts.scheduler.connections);
    branchEdges(D::Protocol,facts.protocol.connections);
    branchEdges(D::Execution,facts.execution.connections);
    if(!edgeIdentityValid || edgeCursor!=result.connections.size())
        return invalid("Composition graph connection identity changed");
    // Source->aggregate edges are already in source products. Preserve order
    // and count self-loops once; never deduplicate by policy or label.
    for(std::size_t position=0;position<result.positions.size();++position){
        auto& incidents=result.positions[position].incidentCapacities;
        std::size_t cursor=0;
        for(const auto& edge:result.connections){
            const auto from=locate(edge.from),to=locate(edge.to);
            if(!from || !to || *from>=result.positions.size() || *to>=result.positions.size() ||
                !edge.policy.queuePolicy.bounded || edge.policy.queuePolicy.capacity==0)
                return invalid("Composition graph edge lacks owned positions or bounded capacity");
            if(*from!=position && *to!=position)continue;
            const auto capacity=static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity);
            if constexpr(Updating){
                if(cursor>=incidents.size())return invalid("Composition graph gained an incident edge");
                incidents[cursor]=capacity;
            }else incidents.push_back(capacity);
            ++cursor;
        }
        if(cursor!=incidents.size())return invalid("Composition graph lost an incident edge");
    }
    return Result::success();
}
} // namespace
::media::Result<MediaCompositionGraphTopology> MediaCompositionGraphTopologyPlanner::plan(const MediaCompositionGraphTopologyFacts& facts,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    if (!resource) return ::media::Result<MediaCompositionGraphTopology>::failure(
        ::media::ErrorInfo::invalidArgument("Composition topology requires a preparation resource"));
    MediaCompositionGraphTopology result(std::move(resource));
    if(auto status=compileTopology<false>(facts,result);!status)
        return ::media::Result<MediaCompositionGraphTopology>::failure(status.error());
    return ::media::Result<MediaCompositionGraphTopology>::success(std::move(result));
}catch(const std::bad_alloc&){return ::media::Result<MediaCompositionGraphTopology>::failure(
    ::media::ErrorInfo::allocationFailed("composition complete topology projection"));}
::media::Status MediaCompositionGraphTopologyPlanner::rebind(
    const MediaCompositionGraphTopologyFacts& facts,MediaCompositionGraphTopology& topology)
{
    return compileTopology<true>(facts,topology);
}
}
