#include "internal/graph/planner/realtime/MediaOutputSegmentPlanner.h"
#include <new>
#include <algorithm>
namespace media::ffmpeg::graph {
using R=MediaOutputSegmentRole;
using S=MediaStreamKind;
using P=MediaPayloadKind;
using Result=::media::Result<MediaOutputSegmentDeclaration>;
MediaNodeKind MediaOutputSegmentPlanner::nodeKind(R role) noexcept {
    switch(role) {
    case R::Scheduler:return MediaNodeKind::AvOutputScheduler;
    case R::Router:return MediaNodeKind::ScheduledOutputRouter;
    case R::TransportPlan:return MediaNodeKind::DatagramTransportPlanSource;
    case R::Sender:return MediaNodeKind::ScheduledDatagramSender;
    case R::VideoMaterializer:case R::AudioMaterializer:return MediaNodeKind::RtpDatagramMaterializer;
    case R::Sdp:return MediaNodeKind::RtpSdpPublisher;
    case R::TsPlan:return MediaNodeKind::ProjectMpegTsPlanSource;
    case R::TsAdapter:return MediaNodeKind::ScheduledTsAccessUnitAdapter;
    case R::TsMux:return MediaNodeKind::FileMux;
    case R::TsMaterializer:return MediaNodeKind::MpegTsDatagramMaterializer;
    default:return MediaNodeKind::Unknown;
    }
}
Result MediaOutputSegmentPlanner::scheduler(const MediaRealtimeEdgePolicySet& p,const MediaEdgePolicy& audio)
try {
    return Result::success({{{R::Scheduler,nodeKind(R::Scheduler)},{R::Router,nodeKind(R::Router)}}, {
        {R::VideoPacket,"",R::Scheduler,"video",S::Video,P::Packet,p.synchronizedPacket,"canonical video -> shared scheduler"},
        {R::AudioPacket,"",R::Scheduler,"audio",S::Audio,P::Packet,audio,"canonical audio -> shared scheduler"},
        {R::Scheduler,"scheduled",R::Router,"scheduled",S::Any,P::Packet,p.synchronizedPacket,"shared scheduler -> typed router"}}});
} catch(const std::bad_alloc&) {return Result::failure(::media::ErrorInfo::allocationFailed("scheduler declaration"));}
Result MediaOutputSegmentPlanner::execution(const MediaRealtimeEdgePolicySet& p)
try {
    return Result::success({{{R::TransportPlan,nodeKind(R::TransportPlan)},{R::Sender,nodeKind(R::Sender)}}, {
        {R::Activation,"",R::TransportPlan,"activation",S::Metadata,P::GraphEvent,p.atomicMetadata,"activation -> datagram transport plan"},
        {R::TransportPlan,"plan",R::Sender,"plan",S::Metadata,P::DatagramTransportPlan,p.atomicMetadata,"transport plan -> common sender"}}});
} catch(const std::bad_alloc&) {return Result::failure(::media::ErrorInfo::allocationFailed("datagram execution declaration"));}
Result MediaOutputSegmentPlanner::rtp(bool audio,P codec,const MediaRealtimeEdgePolicySet& p)
try {
    if (codec!=P::CodecContext && codec!=P::CodecParameters)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP declaration requires codec payload facts"));
    MediaOutputSegmentDeclaration d;
    d.nodes={{R::VideoMaterializer,nodeKind(R::VideoMaterializer)},{R::Sdp,nodeKind(R::Sdp)}};
    if(audio)d.nodes.push_back({R::AudioMaterializer,nodeKind(R::AudioMaterializer)});
    const auto activation=[&](R media) {d.connections.push_back({R::Activation,"",media,"activation",S::Metadata,P::GraphEvent,p.atomicMetadata,"output activation -> scheduled RTP sender"});};
    const auto transport=[&](R media) {d.connections.push_back({R::TransportPlan,"plan",media,"transport_plan",S::Metadata,P::DatagramTransportPlan,p.atomicMetadata,"transport plan -> RTP materializer"});};
    const auto wire=[&](R media) {d.connections.push_back({media,"wire_batch",R::Sender,"batch",S::Metadata,P::WireDatagramBatch,p.synchronizedPacket,"RTP wire datagrams -> shared shaper"});};
    const auto codecEdge=[&](bool video) {d.connections.push_back({video?R::VideoCodec:R::AudioCodec,"",video?R::VideoMaterializer:R::AudioMaterializer,"codec",video?S::Video:S::Audio,codec,p.metadata,video?"video codec -> scheduled RTP sender":"audio codec -> scheduled RTP sender"});};
    const auto packet=[&](bool video) {d.connections.push_back({video?R::VideoPacket:R::AudioPacket,"",video?R::VideoMaterializer:R::AudioMaterializer,"scheduled",video?S::Video:S::Audio,P::Packet,video?p.atomicVideoPacket:p.atomicAudioPacket,video?"scheduled video -> RTP sender":"scheduled audio -> RTP sender"});};
    const auto description=[&](bool video) {d.connections.push_back({video?R::VideoMaterializer:R::AudioMaterializer,"description",R::Sdp,video?"video":"audio",S::Metadata,P::GraphEvent,p.metadata,video?"video sender -> SDP publisher":"audio sender -> SDP publisher"});};
    if(audio) {
        for(const auto media:{R::VideoMaterializer,R::AudioMaterializer}) {activation(media);transport(media);wire(media);}
        codecEdge(true);codecEdge(false);packet(true);packet(false);description(true);description(false);
    } else {
        activation(R::VideoMaterializer);codecEdge(true);packet(true);description(true);transport(R::VideoMaterializer);wire(R::VideoMaterializer);
    }
    return Result::success(std::move(d));
} catch(const std::bad_alloc&) {return Result::failure(::media::ErrorInfo::allocationFailed("RTP output declaration"));}
Result MediaOutputSegmentPlanner::mpegTs(bool audio,bool rtp,P codec,const MediaRealtimeEdgePolicySet& p)
try {
    if(codec!=P::CodecContext && codec!=P::CodecParameters)
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEGTS declaration requires codec payload facts"));
    MediaOutputSegmentDeclaration d;
    for(const auto role:{R::TsPlan,R::TsAdapter,R::TsMux,R::TsMaterializer})d.nodes.push_back({role,nodeKind(role)});
    if(rtp)d.nodes.push_back({R::Sdp,MediaNodeKind::MpegTsRtpSdpPublisher});
    d.connections={
        {R::Activation,"",R::TsPlan,"activation",S::Metadata,P::GraphEvent,p.atomicMetadata,"output activation -> MPEG-TS plan"},
        {R::VideoCodec,"",R::TsMux,"codec",S::Video,codec,p.metadata,"video codec -> MPEG-TS mux"},
        {R::SerializedPacket,"",R::TsAdapter,"scheduled",audio?S::Any:S::Video,P::Packet,p.synchronizedPacket,"scheduled media -> MPEG-TS adapter"},
        {R::TsPlan,"plan",R::TsMux,"plan",S::Metadata,P::ProjectMpegTsRuntimePlan,p.atomicMetadata,"MPEG-TS plan -> mux"},
        {R::TsPlan,"plan",R::TsAdapter,"plan",S::Metadata,P::ProjectMpegTsRuntimePlan,p.atomicMetadata,"MPEG-TS plan -> adapter"},
        {R::TsPlan,"plan",R::TsMaterializer,"protocol_plan",S::Metadata,P::ProjectMpegTsRuntimePlan,p.atomicMetadata,"MPEG-TS plan -> materializer"},
        {R::TsAdapter,"packet",R::TsMux,"packet",S::Any,P::TsAccessUnit,p.synchronizedPacket,"scheduled TS AU -> project mux"},
        {R::TransportPlan,"plan",R::TsPlan,"transport_plan",S::Metadata,P::DatagramTransportPlan,p.atomicMetadata,"transport plan -> MPEG-TS runtime plan"},
        {R::TransportPlan,"plan",R::TsMaterializer,"transport_plan",S::Metadata,P::DatagramTransportPlan,p.atomicMetadata,"transport plan -> MPEG-TS materializer"},
        {R::TsMux,"batch",R::TsMaterializer,"protocol_batch",S::Metadata,P::MpegTsProtocolDatagramBatch,p.synchronizedPacket,"MPEG-TS protocol datagrams -> wire materializer"},
        {R::TsMaterializer,"wire_batch",R::Sender,"batch",S::Metadata,P::WireDatagramBatch,p.synchronizedPacket,"MPEG-TS wire datagrams -> shared shaper"}};
    if(audio)d.connections.insert(std::find_if(d.connections.begin(),d.connections.end(),
        [](const auto& edge){return edge.from==R::TsPlan;}),{R::AudioCodec,"",R::TsMux,"codec",S::Audio,codec,p.metadata,"audio codec -> MPEG-TS mux"});
    if(rtp)d.connections.push_back({R::TsPlan,"plan",R::Sdp,"plan",S::Metadata,P::ProjectMpegTsRuntimePlan,p.atomicMetadata,"MPEG-TS RTP plan -> SDP publisher"});
    return Result::success(std::move(d));
} catch(const std::bad_alloc&) {return Result::failure(::media::ErrorInfo::allocationFailed("MPEGTS output declaration"));}
}
