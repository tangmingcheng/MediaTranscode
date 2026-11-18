#include "internal/graph/planner/realtime/MediaCompositionSourceTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
namespace {
template<bool Updating> ::media::Status compileTopology(
    const MediaCompositionSourceTopologyFacts& facts, MediaCompositionSourceTopology& result)
{
    using Result = ::media::Status;
    using D = MediaSourceTopologyDomain;
    using I = MediaSourceInputRole;
    using S = MediaStartupRouteRole;
    using R = MediaRtpClockRouteRole;
    using V = MediaVideoBranchRole;
    using A = MediaAudioBranchRole;
    using G = MediaCompositionAggregateRole;
    const auto invalid = [](const char* text) { return Result::failure(::media::ErrorInfo::invalidArgument(text)); };
    if ((facts.controlDelivery != MediaRtpControlDelivery::RawProtocolOwner &&
         facts.controlDelivery != MediaRtpControlDelivery::SharedPreparedSession) ||
        facts.clock != MediaAvSyncSourceClockMode::RtpSenderReports)
        return invalid("Source topology requires declared RTP startup facts");
    auto described = MediaStartupRoutePlanner::describe(facts.members, facts.readiness);
    if (!described) return Result::failure(described.error());
    auto startup = MediaStartupRoutePlanner::bind(described.value(), facts.policies.metadata,
        facts.policies.atomicMetadata);
    if (!startup) return Result::failure(startup.error());
    const MediaSourceInputNodeShape input{described.value(), facts.clock};
    const MediaRtpClockRouteShape rtp{facts.members, facts.controlDelivery};
    const bool audio = rtp.has(R::AudioInput);
    const bool decoded = input.has(I::DecoderFrame);
    std::size_t positionCursor = 0, bindingCursor = 0;
    bool identityInvalid = false;
    const auto key = [](D domain, auto role) { return MediaSourceTopologyRole{domain, static_cast<std::size_t>(role)}; };
    const auto locate = [&](MediaSourceTopologyRole role) -> std::optional<std::size_t> {
        const auto end = result.bindings.begin() + static_cast<std::ptrdiff_t>(
            std::min(bindingCursor, result.bindings.size()));
        const auto found = std::find_if(result.bindings.begin(), end,
            [&](const auto& binding) { return binding.role == role; });
        return found == end ? std::nullopt : std::optional{found->position};
    };
    const auto bind = [&](MediaSourceTopologyRole role, std::size_t position) {
        if constexpr (Updating) {
            if (bindingCursor >= result.bindings.size() ||
                result.bindings[bindingCursor].role != role ||
                result.bindings[bindingCursor].position != position) identityInvalid = true;
        } else result.bindings.push_back({role, position});
        ++bindingCursor;
    };
    const auto declare = [&](MediaSourceTopologyRole role, std::optional<MediaNodeKind> kind) {
        bind(role, positionCursor);
        if constexpr (Updating) {
            if (positionCursor >= result.positions.size() || result.positions[positionCursor] != kind)
                identityInvalid = true;
        } else result.positions.push_back(kind);
        ++positionCursor;
    };
    const auto alias = [&](MediaSourceTopologyRole role, MediaSourceTopologyRole target) -> bool {
        const auto position = locate(target);
        if (!position) return false;
        if constexpr (!Updating) if (locate(role)) return false;
        bind(role, *position);
        return !identityInvalid;
    };
    for (const auto role : {R::VideoInput, R::AudioInput, R::Group, R::Fanout, R::VideoBinder, R::AudioBinder, R::StateAdapter, R::VideoControl, R::AudioControl})
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
    if (identityInvalid || positionCursor != result.positions.size() || bindingCursor != result.bindings.size())
        return invalid("Source topology role catalog changed during policy rebinding");
    std::size_t edgeCursor = 0;
    const auto add = [&](MediaSourceTopologyRole from, MediaSourceTopologyRole to, std::string_view output,
        std::string_view port, MediaPayloadKind payload, MediaStreamKind stream, const MediaEdgePolicy& policy) {
        if (identityInvalid) return;
        if constexpr (Updating) {
            if (edgeCursor >= result.connections.size()) { identityInvalid = true; return; }
            auto& edge = result.connections[edgeCursor];
            if (edge.from != from || edge.to != to || edge.output != output || edge.input != port ||
                edge.payload != payload || edge.stream != stream) { identityInvalid = true; return; }
            edge.policy = policy;
        } else result.connections.push_back({from,to,std::pmr::string(output,result.resource().get()),std::pmr::string(port,result.resource().get()),payload,stream,policy});
        ++edgeCursor;
    };
    for (const auto& edge : facts.inputs)
        add(key(D::Input,edge.from),key(D::Input,edge.to),edge.output,edge.input,edge.payload,edge.stream,edge.policy);
    for (const auto& edge : startup.value().shape.connections()) if (startup.value().shape.includes(edge))
        add(key(D::Startup,edge.from),key(D::Startup,edge.to),edge.output,edge.input,
            MediaPayloadKind::GraphEvent,MediaStreamKind::Metadata,startup.value().policy(edge));
    for (const auto& edge : MediaRtpClockRouteConnections) {
        const auto from = rtp.controlSource(edge.from);
        if (rtp.has(from) && rtp.has(edge.to))
            add(key(D::Rtp,from),key(D::Rtp,edge.to),edge.output,edge.input,
                MediaPayloadKind::GraphEvent,MediaStreamKind::Metadata,facts.policies.metadata);
    }
    const auto branch = [&](D domain, const auto& edges) {
        for (const auto& edge : edges) add(key(domain,edge.from),key(domain,edge.to),edge.output,
            edge.input,edge.payload,edge.stream,edge.policy);
    };
    branch(D::VideoDecoder,facts.branches.videoDecoder);
    branch(D::VideoFrames,facts.branches.videoFrames);
    branch(D::Audio,facts.branches.audio);
    for (const auto& edge : facts.aggregate) add(key(D::Aggregate,edge.from),key(D::Aggregate,edge.to),
        edge.output,edge.input,edge.payload,edge.stream,edge.policy);
    if (identityInvalid || edgeCursor != result.connections.size())
        return invalid("Source topology connection catalog changed during policy rebinding");
    if constexpr (Updating) {
        if (result.incidentCapacities.size() != result.positions.size())
            return invalid("Source topology incident position catalog changed");
    } else {
        for (const auto kind : result.positions)
            result.incidentCapacities.push_back(kind ? std::optional{std::pmr::vector<std::uint64_t>{result.resource().get()}} : std::nullopt);
    }
    // Preserve declaration order at each position, without allocating a cursor array.
    for (std::size_t position = 0; position < result.positions.size(); ++position) {
        auto& incidents = result.incidentCapacities[position];
        if (incidents.has_value() != result.positions[position].has_value())
            return invalid("Source topology incident ownership changed");
        std::size_t cursor = 0;
        for (const auto& edge : result.connections) {
            const auto from = locate(edge.from), to = locate(edge.to);
            if (!from || !to || *from >= result.positions.size() || *to >= result.positions.size())
                return invalid("Source topology connection references an undeclared role");
            if (!edge.policy.queuePolicy.bounded || edge.policy.queuePolicy.capacity == 0)
                return invalid("Source topology requires bounded nonempty incident queues");
            if (!incidents || (*from != position && *to != position)) continue;
            const auto capacity = static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity);
            if constexpr (Updating) {
                if (cursor >= incidents->size()) return invalid("Source topology gained an incident edge");
                (*incidents)[cursor] = capacity;
            } else incidents->push_back(capacity);
            ++cursor;
        }
        if (incidents && cursor != incidents->size()) return invalid("Source topology lost an incident edge");
    }
    return Result::success();
}
} // namespace
::media::Result<MediaCompositionSourceTopology> MediaCompositionSourceTopologyPlanner::plan(
    const MediaCompositionSourceTopologyFacts& facts,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    if (!resource) return ::media::Result<MediaCompositionSourceTopology>::failure(
        ::media::ErrorInfo::invalidArgument("Composition topology requires a preparation resource"));
    MediaCompositionSourceTopology result(std::move(resource));
    if (auto status = compileTopology<false>(facts, result); !status)
        return ::media::Result<MediaCompositionSourceTopology>::failure(status.error());
    return ::media::Result<MediaCompositionSourceTopology>::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourceTopology>::failure(
        ::media::ErrorInfo::allocationFailed("composition source topology projection"));
}
::media::Status MediaCompositionSourceTopologyPlanner::rebind(
    const MediaCompositionSourceTopologyFacts& facts, MediaCompositionSourceTopology& topology)
{
    return compileTopology<true>(facts, topology);
}
}
