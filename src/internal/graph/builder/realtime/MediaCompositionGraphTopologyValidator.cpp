#include "internal/graph/builder/realtime/MediaCompositionGraphTopologyValidator.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
::media::Result<MediaCompositionGraphIdentity> MediaCompositionGraphTopologyValidator::validate(const MediaGraph& graph,
    const MediaCompositionGraphTopology& topology, std::span<const MediaEndpoint> endpoints,
    std::span<const std::vector<MediaNodeId>> sourceMembers, std::span<const MediaNodeId> outputMembers)
try {
    using Result=::media::Result<MediaCompositionGraphIdentity>;
    const auto invalid=[](const char* message){return Result::failure(::media::ErrorInfo::invalidArgument(message));};
    if(endpoints.size()!=topology.bindings.size() || graph.nodes().size()!=topology.positions.size())
        return invalid("Composition complete topology node or binding count differs");
    const auto endpoint=[&](MediaCompositionGraphRole role)->const MediaEndpoint*{
        for(std::size_t i=0;i<topology.bindings.size();++i)if(topology.bindings[i].role==role)return &endpoints[i];
        return nullptr;
    };
    std::vector<MediaNodeId> positions(topology.positions.size(),MediaNodeId::invalid());
    for(std::size_t i=0;i<endpoints.size();++i){
        const auto position=topology.bindings[i].position;
        if(position>=positions.size())return invalid("Composition binding position is out of range");
        const auto* node=graph.findNode(endpoints[i].node);
        if(!node || node->kind!=topology.positions[position].kind)
            return invalid("Composition actual node differs from declared kind");
        if(positions[position].isValid() && positions[position]!=node->id)
            return invalid("Composition equivalent roles bind different nodes");
        positions[position]=node->id;
    }
    for(const auto& attachment:topology.attachments){
        const auto* alias=endpoint(attachment.role);
        const auto* owner=endpoint(attachment.owner);
        if(!alias || !owner || alias->node!=owner->node || std::string_view(alias->port)!=std::string_view(attachment.port))
            return invalid("Composition attachment differs from its exact declared endpoint");
    }
    std::size_t members=outputMembers.size();
    for(const auto& source:sourceMembers)members+=source.size();
    if(members!=positions.size())return invalid("Composition source/output membership is not exhaustive");
    for(std::size_t i=0;i<positions.size();++i){
        if(!positions[i].isValid() || std::count(positions.begin(),positions.end(),positions[i])!=1)
            return invalid("Composition owned positions are missing or alias another owner");
        const auto& declared=topology.positions[i];
        if(declared.source && *declared.source>=sourceMembers.size())return invalid("Composition source owner is out of range");
        const auto membership=declared.source?std::span<const MediaNodeId>(sourceMembers[*declared.source]):outputMembers;
        if(std::count(membership.begin(),membership.end(),positions[i])!=1)
            return invalid("Composition owned position differs from processing membership");
        std::pmr::vector<std::uint64_t> planned(declared.incidentCapacities, topology.resource().get());
        std::pmr::vector<std::uint64_t> actual(topology.resource().get());
        for(const auto& edge:graph.edges())if(edge.from.nodeId==positions[i] || edge.to.nodeId==positions[i])
            actual.push_back(static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity));
        std::sort(planned.begin(),planned.end());std::sort(actual.begin(),actual.end());
        if(planned!=actual)return invalid("Composition full incident capacities differ from actual graph");
    }
    std::vector<MediaEdgeId> verified;
    for(const auto& expected:topology.connections){
        const auto* from=endpoint(expected.from),*to=endpoint(expected.to);
        if(!from || !to)return invalid("Composition connection role has no actual endpoint");
        const auto* output=graph.findOutputPort(from->node,expected.output.empty()?from->port:std::string(expected.output));
        const auto* input=graph.findInputPort(to->node,expected.input.empty()?to->port:std::string(expected.input));
        if(!output || !input || output->streamKind!=expected.stream || output->payloadKind!=expected.payload || !input->accepts(*output))
            return invalid("Composition connection differs from actual port contract");
        const MediaEdge* found=nullptr;
        std::size_t producers=0,consumers=0;
        for(const auto& edge:graph.edges()){
            const bool sameInput=edge.to.nodeId==to->node && edge.to.portId==input->id;
            const bool sameOutput=edge.from.nodeId==from->node && edge.from.portId==output->id;
            if(sameInput)++producers;
            if(sameOutput)++consumers;
            if(sameInput && sameOutput){
                if(found)return invalid("Composition graph contains a repeated endpoint pair");
                found=&edge;
            }
        }
        if((!input->multiple && producers!=1) || (!output->multiple && consumers!=1))
            return invalid("Composition actual connections violate port multiplicity");
        if(!found || found->payloadKind!=expected.payload || found->streamKind!=expected.stream || found->policy!=expected.policy ||
            std::find(verified.begin(),verified.end(),found->id)!=verified.end())
            return invalid("Composition actual connection is missing, repeated or differs from declaration");
        verified.push_back(found->id);
    }
    if(verified.size()!=graph.edges().size())return invalid("Composition graph contains undeclared connections");
    return Result::success({std::move(positions),std::move(verified)});
}catch(const std::bad_alloc&){return ::media::Result<MediaCompositionGraphIdentity>::failure(::media::ErrorInfo::allocationFailed("composition complete topology validation"));}
}
