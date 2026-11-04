#pragma once
#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/realtime/MediaOutputSegmentPlanner.h"
#include <array>
namespace media::ffmpeg::graph {
struct MediaOutputSegmentBindings final {
    MediaOutputSegmentDeclaration declaration;
    std::array<MediaEndpoint,static_cast<std::size_t>(MediaOutputSegmentRole::Count)> endpoints;
    MediaEndpoint& endpoint(MediaOutputSegmentRole role) { return endpoints[static_cast<std::size_t>(role)]; }
    const MediaEndpoint& endpoint(MediaOutputSegmentRole role) const { return endpoints[static_cast<std::size_t>(role)]; }
    ::media::Status connect(MediaGraph& graph,std::string_view owner) const {
        for(const auto& declared:declaration.nodes) {
            const auto* node=graph.findNode(endpoint(declared.role).node);
            if(!node || node->kind!=declared.kind)return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Output node differs from declaration"));
        }
        for(const auto& edge:declaration.connections) {
            const auto& from=endpoint(edge.from);
            const auto& to=endpoint(edge.to);
            const auto outputName=edge.output.empty()?from.port:std::string(edge.output);
            const auto* output=graph.findOutputPort(from.node,outputName);
            const auto* input=graph.findInputPort(to.node,std::string(edge.input));
            // Core matching owns Any stream compatibility and port multiplicity.
            if(!output || !input || output->streamKind!=edge.stream || output->payloadKind!=edge.payload || !input->accepts(*output))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Output ports differ from declared connection"));
            if(auto status=MediaGraphBuildSupport::connectChecked(graph,owner,from.node,outputName,to.node,
                    std::string(edge.input),std::string(edge.label),edge.policy);!status)return ::media::Status::failure(status.error());
        }
        return ::media::Status::success();
    }
};
}
