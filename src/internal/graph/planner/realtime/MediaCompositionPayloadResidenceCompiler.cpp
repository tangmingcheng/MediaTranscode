#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaNodeReferenceRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
MediaCompositionGraphRole startupRole(std::size_t source, MediaStartupRouteRole role)
{ return compositionSourceRole(source, {MediaSourceTopologyDomain::Startup, static_cast<std::size_t>(role)}); }
bool sameMembers(const MediaStartupRouteMediaUnits& a, const MediaStartupRouteMediaUnits& b)
{ return a.video.has_value() == b.video.has_value() && a.audio.has_value() == b.audio.has_value(); }

// Compile-time modes share the complete planning walk. Updating instantiates
// no append/resize path and preserves the owned catalog's identities.
template<bool Updating> class ReferenceWriter final {
public:
    explicit ReferenceWriter(MediaCompositionReferenceResidencePlan& result) : m_result(result) {}
    ::media::Status node(const MediaCompositionStartupNodeResidence& value) {
        if constexpr (Updating) {
            if (m_nodes >= m_result.startupNodes.size()) return invalid("Startup node catalog gained an owner");
            auto& old = m_result.startupNodes[m_nodes];
            if (old.position != value.position || old.coordinator != value.coordinator ||
                old.videoPayload != value.videoPayload || !sameMembers(old.units, value.units))
                return invalid("Startup node catalog changed identity or media membership");
            old.units = value.units;
        } else m_result.startupNodes.push_back(value);
        ++m_nodes;
        return ::media::Status::success();
    }
    ::media::Status event(const MediaCompositionStartupEventResidence& value) {
        if constexpr (Updating) {
            if (m_events >= m_result.startupEvents.size()) return invalid("Startup event catalog gained an owner");
            auto& old = m_result.startupEvents[m_events];
            if (old.connection != value.connection || old.coordinator != value.coordinator ||
                old.videoPayload != value.videoPayload || old.units.form != value.units.form ||
                !sameMembers(old.units.media, value.units.media))
                return invalid("Startup event catalog changed identity or media membership");
            old.units = value.units;
        } else m_result.startupEvents.push_back(value);
        ++m_events;
        return ::media::Status::success();
    }
    ::media::Status aggregate(std::size_t position) {
        if constexpr (Updating) {
            if (m_aggregate >= m_result.aggregate.retainingPositions.size() ||
                m_result.aggregate.retainingPositions[m_aggregate] != position)
                return invalid("Aggregate catalog changed its retaining positions");
        } else m_result.aggregate.retainingPositions.push_back(position);
        ++m_aggregate;
        return ::media::Status::success();
    }
    ::media::Status ordinary(std::size_t index, std::optional<std::uint64_t> value) {
        if constexpr (Updating)
            if (m_result.nodeReferences[index].has_value() != value.has_value())
                return invalid("Ordinary node catalog changed owner membership");
        m_result.nodeReferences[index] = value;
        return ::media::Status::success();
    }
    ::media::Status finish() const {
        if (m_nodes != m_result.startupNodes.size() || m_events != m_result.startupEvents.size() ||
            m_aggregate != m_result.aggregate.retainingPositions.size())
            return invalid("Composition residence catalog lost an owner");
        return ::media::Status::success();
    }
private:
    MediaCompositionReferenceResidencePlan& m_result;
    std::size_t m_nodes = 0, m_events = 0, m_aggregate = 0;
};

template<bool Updating> ::media::Status compileReference(
    const MediaCompositionGraphTopology& topology, const MediaCompositionResidenceFacts& facts,
    MediaCompositionReferenceResidencePlan& result)
{
    if (result.nodeReferences.size() != topology.positions.size())
        return invalid("Composition residence catalog has different node slots");
    if constexpr (Updating) {
        if (result.aggregate.kind != MediaPayloadResidenceOwnerKind::Shared || result.aggregate.connection)
            return invalid("Composition aggregate catalog changed owner identity");
    } else {
        result.aggregate.kind = MediaPayloadResidenceOwnerKind::Shared;
        result.aggregate.connection = std::nullopt;
    }
    ReferenceWriter<Updating> writer(result);
    if(facts.sources.empty() || !facts.videoEncoderRetainedFrames || !facts.audioEncoderRetainedFrames || !facts.aggregateReferences)
        return invalid("Composition reference residence lacks finite authoritative holders");
    for(std::size_t source=0;source<facts.sources.size();++source){
        const auto& fact=facts.sources[source];
        const bool audio=fact.route.shape.members==MediaTranscodeStreamSet::AudioVideo;
        if(!fact.videoDescriptorCapacity || audio!=fact.audioAcquiringCapacity.has_value() ||
            audio!=fact.audioDescriptorCapacity.has_value() || (audio && !*fact.audioDescriptorCapacity))
            return invalid("Composition source residence lacks exact input holder facts");
        auto media=MediaStartupRouteMediaPlanner::plan(fact.route,fact.videoBatchUnits,fact.audioBatchUnits);
        if(!media)return ::media::Status::failure(media.error());
        const auto coordinator=topology.position(startupRole(source,MediaStartupRouteRole::Coordinator));
        if(!coordinator)return invalid("Composition startup coordinator position is absent");
        for(std::size_t role=0;role<media.value().nodes.size();++role){
            if(!media.value().nodes[role])continue;
            const auto owned=topology.position(startupRole(source,static_cast<MediaStartupRouteRole>(role)));
            if(!owned || *owned>=topology.positions.size())return invalid("Composition typed startup node is absent");
            if (auto status = writer.node({*owned,*coordinator,media.value().videoPayload,*media.value().nodes[role]}); !status)
                return status;
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
            if (auto status = writer.event({*match,*coordinator,media.value().videoPayload,*media.value().events[route]}); !status)
                return status;
        }
    }
    for(const auto role:{MediaCompositionAggregateRole::VideoAggregate,MediaCompositionAggregateRole::AudioAggregate}){
        const auto owned=topology.position(compositionOutputRole(MediaCompositionGraphDomain::Aggregate,role));
        if(!owned)return invalid("Composition aggregate retaining position is absent");
        if (auto status = writer.aggregate(*owned); !status) return status;
    }
    result.aggregate.maximumReferences=facts.aggregateReferences;
    for(std::size_t index=0;index<topology.positions.size();++index){
        const auto& owned=topology.positions[index];
        if(std::find(result.aggregate.retainingPositions.begin(),result.aggregate.retainingPositions.end(),index)!=result.aggregate.retainingPositions.end() ||
            std::any_of(result.startupNodes.begin(),result.startupNodes.end(),[&](const auto& node){return node.position==index;})) {
            if (auto status = writer.ordinary(index, std::nullopt); !status) return status;
            continue;
        }
        std::optional<std::uint64_t> acquiring,descriptors,filter;
        if(owned.source){
            if(*owned.source>=facts.sources.size())return invalid("Composition residence source is absent");
            const auto& source=facts.sources[*owned.source];
            const auto rtp=[&](MediaRtpClockRouteRole role){return topology.position(compositionSourceRole(*owned.source,
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
        if(!retained)return ::media::Status::failure(retained.error());
        if(owned.kind==MediaNodeKind::VideoEncode || owned.kind==MediaNodeKind::AudioEncode){
            retained=MediaCheckedArithmetic::add(retained.value(),owned.kind==MediaNodeKind::VideoEncode?
                facts.videoEncoderRetainedFrames:facts.audioEncoderRetainedFrames,"composition prepared encoder reference retention");
            if(!retained)return ::media::Status::failure(retained.error());
        }
        if (auto status = writer.ordinary(index, retained.value()); !status) return status;
    }
    return writer.finish();
}
} // namespace

::media::Result<MediaCompositionReferenceResidencePlan> MediaCompositionPayloadResidencePlanner::compile(
    const MediaCompositionGraphTopology& topology, const MediaCompositionResidenceFacts& facts)
try {
    using Result = ::media::Result<MediaCompositionReferenceResidencePlan>;
    MediaCompositionReferenceResidencePlan result;
    result.nodeReferences.resize(topology.positions.size());
    if (auto status = compileReference<false>(topology, facts, result); !status)
        return Result::failure(status.error());
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionReferenceResidencePlan>::failure(
        ::media::ErrorInfo::allocationFailed("composition pure reference residence"));
}

::media::Status MediaCompositionPayloadResidencePlanner::rebind(
    const MediaCompositionGraphTopology& topology, const MediaCompositionResidenceFacts& facts,
    MediaCompositionReferenceResidencePlan& result)
{ return compileReference<true>(topology, facts, result); }
} // namespace media::ffmpeg::graph
