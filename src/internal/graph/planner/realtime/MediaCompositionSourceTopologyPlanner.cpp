#include "internal/graph/planner/realtime/MediaCompositionSourceTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
::media::Result<MediaCompositionSourceTopology> MediaCompositionSourceTopologyPlanner::plan(
    const MediaCompositionSourceTopologyFacts& facts)
try {
    using Result = ::media::Result<MediaCompositionSourceTopology>;
    using D = MediaSourceTopologyDomain;
    using I = MediaSourceInputRole;
    using S = MediaStartupRouteRole;
    using R = MediaRtpClockRouteRole;
    using V = MediaVideoBranchRole;
    using A = MediaAudioBranchRole;
    using G = MediaCompositionAggregateRole;
    const auto invalid = [](const char* text) { return Result::failure(::media::ErrorInfo::invalidArgument(text)); };
    if (facts.clock != MediaAvSyncSourceClockMode::RtpSenderReports)
        return invalid("Source topology requires declared RTP startup facts");
    auto described = MediaStartupRoutePlanner::describe(facts.members, facts.readiness);
    if (!described) return Result::failure(described.error());
    auto startup = MediaStartupRoutePlanner::bind(described.value(), facts.policies.metadata,
        facts.policies.atomicMetadata);
    if (!startup) return Result::failure(startup.error());
    const MediaSourceInputNodeShape input{described.value(), facts.clock};
    const MediaRtpClockRouteShape rtp{facts.members};
    const bool audio = rtp.has(R::AudioInput);
    const bool decoded = input.has(I::DecoderFrame);
    MediaCompositionSourceTopology result;
    const auto key = [](D domain, auto role) { return MediaSourceTopologyRole{domain, static_cast<std::size_t>(role)}; };
    const auto locate = [&](MediaSourceTopologyRole role) -> std::optional<std::size_t> {
        const auto found = std::find_if(result.bindings.begin(), result.bindings.end(),
            [&](const auto& binding) { return binding.role == role; });
        return found == result.bindings.end() ? std::nullopt : std::optional{found->position};
    };
    const auto declare = [&](MediaSourceTopologyRole role, std::optional<MediaNodeKind> kind) {
        result.bindings.push_back({role, result.positions.size()});
        result.positions.push_back(kind);
    };
    const auto alias = [&](MediaSourceTopologyRole role, MediaSourceTopologyRole target) -> bool {
        const auto position = locate(target);
        if (!position || locate(role)) return false;
        result.bindings.push_back({role, *position});
        return true;
    };
    for (const auto role : {R::VideoInput, R::AudioInput, R::Group, R::Fanout, R::VideoBinder, R::AudioBinder, R::StateAdapter})
        if (rtp.has(role)) declare(key(D::Rtp, role), rtp.nodeKind(role));
    for (const auto role : {S::VideoInput, S::AudioInput, S::Coordinator, S::Binder, S::Sequencer, S::Extractor})
        if (role != S::AudioInput || audio) declare(key(D::Startup, role), described.value().nodeKind(role));
    for (const auto role : {I::SourceClock, I::VideoGate, I::AudioGate, I::StartupClock})
        if (input.has(role)) declare(key(D::Input, role), input.ownedKind(role));
    for (const auto role : {I::VideoStartup, I::AudioStartup, I::Coordinator})
        if (input.has(role) && !alias(key(D::Input, role), key(D::Startup, *input.startupRole(role))))
            return invalid("Source startup alias is absent or repeated");
    for (const auto pair : {std::pair{I::RawVideo,R::VideoInput}, {I::RawAudio,R::AudioInput},
            {I::ProtocolVideo,R::VideoBinder}, {I::ProtocolAudio,R::AudioBinder}, {I::ProtocolClock,R::StateAdapter}})
        if (input.has(pair.first) && !alias(key(D::Input,pair.first), key(D::Rtp,pair.second)))
            return invalid("Source protocol alias is absent or repeated");
    // Output codecs/aggregates are shared external boundaries, not source owners.
    for (const auto role : {G::OutputVideoCodec, G::VideoAggregate, G::AudioAggregate}) declare(key(D::Aggregate,role),std::nullopt);
    if (audio) declare(key(D::Aggregate,G::OutputAudioCodec),std::nullopt);
    const auto& decoder = facts.branches.videoDecoderShape;
    const auto& frames = facts.branches.videoFrameShape;
    if (audio != facts.branches.audioShape.has_value())
        return invalid("Source topology audio declaration differs from membership");
    const auto owned = [&](D domain, const auto& shape, auto count) {
        using Role = decltype(count);
        for (std::size_t i=0; i<static_cast<std::size_t>(count); ++i) {
            const auto role=static_cast<Role>(i);
            if (shape.has(role)) if (const auto kind=shape.ownedKind(role)) declare(key(domain,role),kind);
        }
    };
    owned(D::VideoDecoder,decoder,V::Count);
    owned(D::VideoFrames,frames,V::Count);
    if (audio) owned(D::Audio,*facts.branches.audioShape,A::Count);
    if (!alias(key(D::VideoDecoder,V::Format),key(D::Rtp,R::VideoInput)) ||
        !alias(key(D::VideoDecoder,V::Packet), decoded ? key(D::Input,I::VideoGate) : key(D::Startup,S::Extractor)) ||
        !alias(key(D::VideoFrames,V::Frame), decoded ? key(D::Startup,S::Extractor) : key(D::VideoDecoder,V::Decode)) ||
        !alias(key(D::VideoFrames,V::OutputCodec),key(D::Aggregate,G::OutputVideoCodec)) ||
        !alias(key(D::Aggregate,G::SourceVideoFrame),key(D::VideoFrames,V::Preparation)) ||
        !alias(key(D::Aggregate,G::SourceActivated),key(D::Startup,S::Sequencer)))
        return invalid("Source video alias cannot bind its declared producer");
    if (decoded && !alias(key(D::Input,I::DecoderFrame),key(D::VideoDecoder,V::Decode)))
        return invalid("Source decoded startup alias cannot bind decoder");
    if (audio && (!alias(key(D::Audio,A::Format),key(D::Rtp,R::AudioInput)) ||
        !alias(key(D::Audio,A::Packet),key(D::Startup,S::Extractor)) ||
        !alias(key(D::Audio,A::OutputCodec),key(D::Aggregate,G::OutputAudioCodec)) ||
        !alias(key(D::Aggregate,G::SourceAudioFrame),key(D::Audio,A::Resample))))
        return invalid("Source audio alias cannot bind its declared producer");
    const auto add = [&](MediaSourceTopologyRole from, MediaSourceTopologyRole to, std::string output,
        std::string port, MediaPayloadKind payload, MediaStreamKind stream, const MediaEdgePolicy& policy) {
        result.connections.push_back({from,to,std::move(output),std::move(port),payload,stream,policy});
    };
    for (const auto& edge : facts.inputs)
        add(key(D::Input,edge.from),key(D::Input,edge.to),std::string(edge.output),std::string(edge.input),edge.payload,edge.stream,edge.policy);
    for (const auto& edge : startup.value().shape.connections()) if (startup.value().shape.includes(edge))
        add(key(D::Startup,edge.from),key(D::Startup,edge.to),std::string(edge.output),std::string(edge.input),
            MediaPayloadKind::GraphEvent,MediaStreamKind::Metadata,startup.value().policy(edge));
    for (const auto& edge : MediaRtpClockRouteConnections) if (rtp.has(edge.from) && rtp.has(edge.to))
        add(key(D::Rtp,edge.from),key(D::Rtp,edge.to),edge.output,edge.input,
            MediaPayloadKind::GraphEvent,MediaStreamKind::Metadata,facts.policies.metadata);
    const auto branch = [&](D domain, const auto& edges) {
        for (const auto& edge : edges) add(key(domain,edge.from),key(domain,edge.to),std::string(edge.output),
            std::string(edge.input),edge.payload,edge.stream,edge.policy);
    };
    branch(D::VideoDecoder,facts.branches.videoDecoder);
    branch(D::VideoFrames,facts.branches.videoFrames);
    branch(D::Audio,facts.branches.audio);
    for (const auto& edge : facts.aggregate) add(key(D::Aggregate,edge.from),key(D::Aggregate,edge.to),
        edge.output,edge.input,edge.payload,edge.stream,edge.policy);
    for (const auto kind : result.positions)
        result.incidentCapacities.push_back(kind ? std::optional{std::vector<std::uint64_t>{}} : std::nullopt);
    for (const auto& edge : result.connections) {
        const auto from=locate(edge.from);
        const auto to=locate(edge.to);
        if (!from || !to) return invalid("Source topology connection references an undeclared role");
        if (!edge.policy.queuePolicy.bounded || edge.policy.queuePolicy.capacity==0)
            return invalid("Source topology requires bounded nonempty incident queues");
        const auto capacity=static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity);
        if (result.incidentCapacities[*from]) result.incidentCapacities[*from]->push_back(capacity);
        if (*to!=*from && result.incidentCapacities[*to]) result.incidentCapacities[*to]->push_back(capacity);
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourceTopology>::failure(
        ::media::ErrorInfo::allocationFailed("composition source topology projection"));
}
}
