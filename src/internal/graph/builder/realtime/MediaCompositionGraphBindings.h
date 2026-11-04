#pragma once
#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include "internal/graph/builder/MediaEndpoint.h"
namespace media::ffmpeg::graph {
// Actual identities only. Endpoint attachment choices belong to the pure plan.
class MediaCompositionGraphBindings final {
public:
    explicit MediaCompositionGraphBindings(const MediaCompositionGraphTopology& topology)
        : topology_(topology), endpoints_(topology.bindings.size()) {}
    template<class Resolve>
    void bindOutput(MediaCompositionGraphDomain domain, const Resolve& resolve) {
        for(std::size_t i=0;i<topology_.bindings.size();++i){
            const auto& role=topology_.bindings[i].role;
            if(role.domain==domain && !role.source)endpoints_[i]=resolve(role.role);
        }
    }
    ::media::Status bindSource(std::size_t source,const MediaCompositionSourceTopology& topology,std::span<const MediaEndpoint> endpoints) {
        if(topology.bindings.size()!=endpoints.size())return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Composition source binding count differs"));
        for(std::size_t i=0;i<topology.bindings.size();++i){
            const auto role=compositionSourceRole(source,topology.bindings[i].role);
            bool found=false;
            for(std::size_t j=0;j<topology_.bindings.size();++j)if(topology_.bindings[j].role==role){endpoints_[j]=endpoints[i];found=true;}
            if(!found)return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Composition source role is undeclared"));
        }
        return ::media::Status::success();
    }
    MediaEndpoint endpoint(MediaCompositionGraphRole role) const {
        for(std::size_t i=0;i<topology_.bindings.size();++i)if(topology_.bindings[i].role==role)return endpoints_[i];
        return {};
    }
    MediaEndpoint attached(MediaCompositionGraphRole role) const {
        const auto* attachment=topology_.attachment(role);
        if(!attachment)return endpoint(role);
        return {endpoint(attachment->owner).node,attachment->port};
    }
    std::span<const MediaEndpoint> endpoints() const noexcept { return endpoints_; }
private:
    const MediaCompositionGraphTopology& topology_;
    std::vector<MediaEndpoint> endpoints_;
};
}
