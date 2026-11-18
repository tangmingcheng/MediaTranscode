#pragma once
#include "internal/graph/builder/MediaEndpoint.h"
#include <optional>
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/video/MediaVideoBranchConnectionsPlanner.h"
#include "internal/graph/planner/audio/MediaAudioBranchConnectionsPlanner.h"

namespace media::ffmpeg::graph {
// These arrays are populated from actual allocated node IDs and supplied endpoints.
template<class Role>
struct MediaBranchBindings final {
    std::array<MediaEndpoint, static_cast<std::size_t>(Role::Count)> endpoints;
    template<class Shape>
    ::media::Status validateDeclaration(const MediaGraph& graph, const Shape& shape) const {
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const auto role = static_cast<Role>(i);
            const auto kind = Shape::ownedKind(role);
            const auto* node = graph.findNode(endpoints[i].node);
            if (kind) {
                if (shape.has(role) != endpoints[i].node.isValid() ||
                    (shape.has(role) && (!node || node->kind != *kind)))
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Source branch node differs from its declaration"));
                if (node) for (std::size_t j = 0; j < i; ++j)
                    if (Shape::ownedKind(static_cast<Role>(j)) && endpoints[j].node == node->id)
                        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                            "Source branch owned roles alias the same node"));
            } else if (shape.has(role) && (!node || !graph.findOutputPort(node->id, endpoints[i].port))) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Source branch required external endpoint is absent"));
            }
        }
        return ::media::Status::success();
    }
    template<class Connections>
    ::media::Status connect(MediaGraph& graph,const Connections& connections,const std::string& prefix,const char* owner) const {
        for(const auto& edge:connections) {
            const auto from=endpoint(edge.from);
            const auto to=endpoint(edge.to);
            const auto outputName=edge.output.empty()?from.port:std::string(edge.output);
            const auto* output=graph.findOutputPort(from.node,outputName);
            const auto* input=graph.findInputPort(to.node,std::string(edge.input));
            if(!output || !input || output->streamKind!=edge.stream || input->streamKind!=edge.stream ||
                output->payloadKind!=edge.payload || input->payloadKind!=edge.payload)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Branch ports differ from declared stream and payload"));
            if(auto status=MediaGraphBuildSupport::connectChecked(graph,owner,from.node,outputName,to.node,
                    std::string(edge.input),prefix+std::string(edge.label),edge.policy);!status)
                return ::media::Status::failure(status.error());
        }
        return ::media::Status::success();
    }
    MediaEndpoint endpoint(Role role) const {
        const auto index = static_cast<std::size_t>(role);
        return index < endpoints.size() ? endpoints[index] : MediaEndpoint{};
    }
};
using MediaVideoBranchBindings = MediaBranchBindings<MediaVideoBranchRole>;
using MediaAudioBranchBindings = MediaBranchBindings<MediaAudioBranchRole>;
struct MediaVideoSourceBranchBindings final {
    std::optional<MediaVideoBranchBindings> decoder;
    MediaVideoBranchBindings frames;
};
} // namespace media::ffmpeg::graph
