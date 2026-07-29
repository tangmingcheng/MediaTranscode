#include "internal/graph/builder/segments/MediaDemuxClockInputSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

using Support = MediaRealtimeAvSyncInputGraphSupport;

} // namespace

::media::Result<MediaDemuxClockInputEndpoints>
MediaDemuxClockInputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaDemuxClockInputSegmentOptions& options,
    const MediaDemuxTimestampInputClockAssemblyPlan& plan)
{
    if (options.prefix.empty() || !options.videoPacket.valid() ||
        !options.audioPacket.valid() || !options.syncGroup.valid()) {
        return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux clock input segment requires complete planned endpoints"));
    }
    auto video = Support::addNode(
        graph,
        MediaNodeKind::DemuxPacketClockBinder,
        options.prefix + ".video.demux_clock_binder",
        "URL demux video timestamp binder");
    auto audio = Support::addNode(
        graph,
        MediaNodeKind::DemuxPacketClockBinder,
        options.prefix + ".audio.demux_clock_binder",
        "URL demux audio timestamp binder");
    if (!video || !audio) {
        return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
            !video ? video.error() : audio.error());
    }
    for (const auto [node, stream] : {
             std::pair{video.value(), MediaStreamKind::Video},
             std::pair{audio.value(), MediaStreamKind::Audio}}) {
        if (auto status = Support::addInput(
                graph, node, "packet", stream,
                MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
            !status) {
            return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
                status.error());
        }
        if (auto status = Support::addOutput(
                graph, node, "packet", stream,
                MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
            !status) {
            return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
                status.error());
        }
        if (auto status =
                MediaRealtimeAvSyncNodeConfigurator::
                    configureDemuxPacketClockBinder(
                        graph, node, stream, options.syncGroup, plan);
            !status) {
            return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
                status.error());
        }
    }
    if (auto status = Support::addOutput(
            graph, video.value(), "state", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent,
            true, true); !status) {
        return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
            status.error());
    }
    if (auto status = Support::connect(
            graph, options.videoPacket, video.value(), "packet",
            "URL video packet -> demux timestamp binder",
            options.packetPolicy); !status) {
        return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
            status.error());
    }
    if (auto status = Support::connect(
            graph, options.audioPacket, audio.value(), "packet",
            "URL audio packet -> demux timestamp binder",
            options.packetPolicy); !status) {
        return ::media::Result<MediaDemuxClockInputEndpoints>::failure(
            status.error());
    }
    return ::media::Result<MediaDemuxClockInputEndpoints>::success(
        MediaDemuxClockInputEndpoints{
            MediaEndpoint{video.value(), "packet"},
            MediaEndpoint{audio.value(), "packet"},
            MediaEndpoint{video.value(), "state"}});
}

} // namespace media::ffmpeg::graph
