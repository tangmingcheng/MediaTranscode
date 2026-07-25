#include "unit/fixtures/ScheduledRtpOutputIntegrationGraph.h"

#include "common/AvSyncRuntimeTestSupport.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"

#include <utility>

namespace media_transcode::test {
namespace {

using namespace ::media::ffmpeg::graph;

MediaEndpoint addSource(
    MediaGraph& graph,
    const char* name,
    const char* port,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload,
    bool multiple = false)
{
    const MediaNodeId node = graph.addNode(MediaNodeKind::DebugDump, name);
    graph.addOutputPort(
        node, port, stream, edge, payload, true, multiple);
    return {node, port};
}

MediaNodeId findOnlyNode(const MediaGraph& graph, MediaNodeKind kind)
{
    MediaNodeId found;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind != kind) continue;
        if (found.isValid()) return {};
        found = node.id;
    }
    return found;
}

} // namespace

::media::Result<ScheduledRtpOutputIntegrationGraph>
ScheduledRtpOutputIntegrationGraphBuilder::build(
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    ScheduledRtpOutputIntegrationGraph fixture;
    const MediaEndpoint epoch = addSource(
        fixture.graph, "decode-epoch", "activated",
        MediaStreamKind::Metadata, MediaEdgeKind::Event,
        MediaPayloadKind::GraphEvent, true);
    const MediaEndpoint videoCodec = addSource(
        fixture.graph, "decode-video-codec", "codec",
        MediaStreamKind::Video, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);
    const MediaEndpoint audioCodec = addSource(
        fixture.graph, "decode-audio-codec", "codec",
        MediaStreamKind::Audio, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);
    const MediaEndpoint canonicalVideo = addSource(
        fixture.graph, "decode-canonical-video", "canonical",
        MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);
    const MediaEndpoint canonicalAudio = addSource(
        fixture.graph, "decode-canonical-audio", "canonical",
        MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);

    auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(
        fixture.graph,
        {"decode-scheduling", canonicalVideo, canonicalAudio}, plan);
    if (!scheduled) {
        return ::media::Result<ScheduledRtpOutputIntegrationGraph>::failure(
            scheduled.error());
    }
    fixture.scheduler = findOnlyNode(
        fixture.graph, MediaNodeKind::AvOutputScheduler);
    fixture.router = scheduled.value().video.node;
    if (!fixture.scheduler.isValid() || !fixture.router.isValid() ||
        fixture.router != scheduled.value().audio.node) {
        return ::media::Result<ScheduledRtpOutputIntegrationGraph>::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP integration graph has invalid scheduling authority"));
    }

    fixture.binder = fixture.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "decode-binder");
    if (!fixture.binder.isValid() ||
        !fixture.graph.setNodeOption(
            fixture.binder, "playback_epoch_binder.sync_group",
            plan.groupKey.value())) {
        return ::media::Result<ScheduledRtpOutputIntegrationGraph>::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP integration graph could not add its epoch binder"));
    }
    addPlaybackEpochReleaseBoundary(fixture.graph, fixture.binder);

    auto output = MediaScheduledRtpOutputSegmentBuilder::build(
        fixture.graph,
        {"decode-output", epoch, videoCodec, audioCodec,
         scheduled.value().video, scheduled.value().audio},
        plan);
    if (!output) {
        return ::media::Result<ScheduledRtpOutputIntegrationGraph>::failure(
            output.error());
    }
    fixture.videoSender = output.value().videoSender;
    fixture.audioSender = output.value().audioSender;
    fixture.publisher = output.value().sdpPublisher;
    return ::media::Result<ScheduledRtpOutputIntegrationGraph>::success(
        std::move(fixture));
}

} // namespace media_transcode::test
