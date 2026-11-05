#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaNodeReferenceRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <limits>
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
class CompositionResidenceView final : public MediaPayloadResidenceTopologyView {
public:
    CompositionResidenceView(const MediaCompositionGraphTopology& topology,
        const MediaCompositionReferenceResidencePlan& reference, MediaStreamKind stream,
        MediaPayloadKind payload, std::size_t owners)
        : m_topology(topology), m_reference(reference), m_stream(stream), m_payload(payload), m_owners(owners) {}
    std::size_t nodeCount() const noexcept override { return m_topology.positions.size(); }
    std::size_t connectionSlots() const noexcept override { return m_topology.connections.size(); }
    std::size_t ownerSlots() const noexcept override { return m_owners; }
    ::media::Result<std::optional<MediaPayloadResidenceConnection>> connectionAt(std::size_t index) const override {
        using R = ::media::Result<std::optional<MediaPayloadResidenceConnection>>;
        auto edge = projectEdge(index);
        if (!edge) return R::failure(edge.error());
        return R::success(edge.value() ? std::optional{edge.value()->connection} : std::nullopt);
    }
    ::media::Result<std::optional<MediaPayloadResidenceOwnerView>> ownerAt(std::size_t index) const override {
        using R = ::media::Result<std::optional<MediaPayloadResidenceOwnerView>>;
        if (!index) {
            const auto& owner = m_reference.aggregate;
            return R::success(MediaPayloadResidenceOwnerView{std::span<const std::size_t>{owner.retainingPositions},
                owner.maximumReferences, owner.kind, owner.connection});
        }
        --index;
        if (index < connectionSlots()) {
            auto edge = projectEdge(index);
            if (!edge) return R::failure(edge.error());
            if (!edge.value()) return R::success(std::nullopt);
            return R::success(MediaPayloadResidenceOwnerView{edge.value()->connection.from,
                edge.value()->references, edge.value()->kind, index});
        }
        index -= connectionSlots();
        if (index < m_reference.nodeReferences.size()) {
            const auto count = m_reference.nodeReferences[index];
            if (!count) return R::success(std::nullopt);
            return R::success(MediaPayloadResidenceOwnerView{index, *count, MediaPayloadResidenceOwnerKind::Node, std::nullopt});
        }
        index -= m_reference.nodeReferences.size();
        const auto& node = m_reference.startupNodes[index];
        const auto count = mediaUnits(m_stream, m_payload, node.videoPayload, node.units);
        if (!count) return R::success(std::nullopt);
        return R::success(MediaPayloadResidenceOwnerView{node.position, *count,
            MediaPayloadResidenceOwnerKind::StartupNode, std::nullopt});
    }
private:
    struct EdgeProjection final {
        MediaPayloadResidenceConnection connection;
        std::uint64_t references;
        MediaPayloadResidenceOwnerKind kind;
    };
    ::media::Result<std::optional<EdgeProjection>> projectEdge(std::size_t index) const {
        using R = ::media::Result<std::optional<EdgeProjection>>;
        const auto& edge = m_topology.connections[index];
        const auto typed = std::find_if(m_reference.startupEvents.begin(), m_reference.startupEvents.end(),
            [&](const auto& value) { return value.connection == index; });
        const auto units = typed == m_reference.startupEvents.end() ? std::optional<std::uint64_t>{}
            : mediaUnits(m_stream, m_payload, typed->videoPayload, typed->units.media);
        const bool carries = typed == m_reference.startupEvents.end()
            ? edge.stream == m_stream && edge.payload == m_payload : units.has_value();
        if (!carries) return R::success(std::nullopt);
        const auto from = position(m_topology, edge.from), to = position(m_topology, edge.to);
        if (!from || !to) return R::failure(::media::ErrorInfo::invalidArgument("Composition residence edge lacks positions"));
        auto count = typed == m_reference.startupEvents.end()
            ? ::media::Result<std::uint64_t>::success(static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity))
            : MediaCheckedArithmetic::multiply(typed->units.wrapperSlots, *units, "composition startup wrapper media references");
        if (!count) return R::failure(count.error());
        return R::success(EdgeProjection{{*from, *to}, count.value(), typed == m_reference.startupEvents.end()
            ? MediaPayloadResidenceOwnerKind::Edge : MediaPayloadResidenceOwnerKind::StartupEdge});
    }
    const MediaCompositionGraphTopology& m_topology;
    const MediaCompositionReferenceResidencePlan& m_reference;
    MediaStreamKind m_stream;
    MediaPayloadKind m_payload;
    std::size_t m_owners;
};
::media::Result<CompositionResidenceView> residenceView(
    const MediaCompositionGraphTopology& topology, const MediaCompositionReferenceResidencePlan& reference,
    MediaStreamKind stream, MediaPayloadKind payload)
{
    if(reference.nodeReferences.size()!=topology.positions.size() ||
        (stream!=MediaStreamKind::Video && stream!=MediaStreamKind::Audio) ||
        (payload!=MediaPayloadKind::Packet && payload!=MediaPayloadKind::Frame))
        return ::media::Result<CompositionResidenceView>::failure(::media::ErrorInfo::invalidArgument("Composition residence domain is invalid"));
    std::size_t owners = 1; // The already-planned shared aggregate owner.
    for (const auto count : {topology.connections.size(), reference.nodeReferences.size(), reference.startupNodes.size()}) {
        if (count > (std::numeric_limits<std::size_t>::max)() - owners)
            return ::media::Result<CompositionResidenceView>::failure(::media::ErrorInfo::allocationFailed("Composition residence owner slots overflow"));
        owners += count;
    }
    return ::media::Result<CompositionResidenceView>::success(
        CompositionResidenceView(topology, reference, stream, payload, owners));
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
    std::size_t producerPosition,MediaStreamKind stream,MediaPayloadKind payload,
    MediaPayloadResidenceSelection selection)
try {
    using Result=::media::Result<MediaPayloadResidenceProjection>;
    auto view = residenceView(topology, reference, stream, payload);
    if (!view) return Result::failure(view.error());
    return MediaGraphPayloadResidencePlanner::plan(view.value(),producerPosition,selection);
}catch(const std::bad_alloc&){return ::media::Result<MediaPayloadResidenceProjection>::failure(
    ::media::ErrorInfo::allocationFailed("composition pure payload residence"));}

::media::Status MediaCompositionPayloadResidencePlanner::rebindOwner(
    const MediaCompositionGraphTopology& topology, const MediaCompositionReferenceResidencePlan& reference,
    MediaStreamKind stream, MediaPayloadKind payload, MediaPayloadResidenceOwner& owner)
{
    const auto invalid = [] { return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Composition residence owner differs from its fixed projection identity")); };
    auto view = residenceView(topology, reference, stream, payload);
    if (!view) return ::media::Status::failure(view.error());
    if (!owner.projectionSlot || *owner.projectionSlot >= view.value().ownerSlots()) return invalid();
    auto current = view.value().ownerAt(*owner.projectionSlot);
    if (!current) return ::media::Status::failure(current.error());
    if (!current.value()) return invalid();
    const auto& value = *current.value();
    if (value.kind != owner.kind || value.connection != owner.connection ||
        value.positionCount() != owner.retainingPositions.size()) return invalid();
    for (std::size_t i = 0; i < value.positionCount(); ++i)
        if (value.positionAt(i) >= topology.positions.size() ||
            value.positionAt(i) != owner.retainingPositions[i]) return invalid();
    owner.maximumReferences = value.maximumReferences;
    return ::media::Status::success();
}
}
