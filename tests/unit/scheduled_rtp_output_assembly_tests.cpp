#include "unit/fixtures/ScheduledRtpOutputNodeTestSupport.h"

#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include <memory>
#include <utility>

namespace media_transcode::test::scheduled_rtp_output {

using namespace media::ffmpeg::graph;

namespace {

void testProductionNodeKinds(TestContext& ctx)
{
    EXPECT_EQ(ctx, MediaScheduledRtpSenderNode::staticKind(),
              MediaNodeKind::ScheduledRtpSender);
    EXPECT_EQ(ctx, MediaDualMediaSdpPublisherNode::staticKind(),
              MediaNodeKind::DualMediaSdpPublisher);
}

void testBuilderAndCompilerInjectExactRegisteredGroup(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto runtimePlan = std::move(*outer.value().avSyncRuntime);
    MediaGraph graph;
    const MediaNodeId epoch = graph.addNode(MediaNodeKind::DebugDump, "epoch");
    const MediaNodeId videoCodec = graph.addNode(
        MediaNodeKind::DebugDump, "video-codec");
    const MediaNodeId audioCodec = graph.addNode(
        MediaNodeKind::DebugDump, "audio-codec");
    const MediaNodeId videoScheduled = graph.addNode(
        MediaNodeKind::DebugDump, "video-scheduled");
    const MediaNodeId audioScheduled = graph.addNode(
        MediaNodeKind::DebugDump, "audio-scheduled");
    graph.addOutputPort(
        epoch, "activated", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    graph.addOutputPort(
        videoCodec, "codec", MediaStreamKind::Video,
        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(
        audioCodec, "codec", MediaStreamKind::Audio,
        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(
        videoScheduled, "scheduled", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
    graph.addOutputPort(
        audioScheduled, "scheduled", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
    auto built = MediaScheduledRtpOutputSegmentBuilder::build(
        graph,
        MediaScheduledRtpOutputSegmentOptions{
            "task8", {epoch, "activated"}, {videoCodec, "codec"},
            {audioCodec, "codec"}, {videoScheduled, "scheduled"},
            {audioScheduled, "scheduled"}},
        runtimePlan);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(graph).ok());
    const MediaNode* videoSender = graph.findNode(built.value().videoSender);
    const MediaNode* audioSender = graph.findNode(built.value().audioSender);
    EXPECT_TRUE(ctx, videoSender && audioSender);
    if (!videoSender || !audioSender) return;
    auto decodedVideo = MediaScheduledRtpSenderNodePlanCodec::decode(
        *videoSender);
    auto decodedAudio = MediaScheduledRtpSenderNodePlanCodec::decode(
        *audioSender);
    EXPECT_TRUE(ctx, decodedVideo && decodedAudio);
    if (decodedVideo && decodedAudio) {
        EXPECT_FALSE(ctx, decodedVideo.value().output.ssrc ==
                              decodedAudio.value().output.ssrc);
        EXPECT_FALSE(
            ctx,
            decodedVideo.value().output.transport.remoteRtpEndpoint().port() ==
                decodedAudio.value().output.transport.remoteRtpEndpoint().port());
        EXPECT_EQ(ctx, decodedVideo.value().sdp.cname,
                  decodedAudio.value().sdp.cname);
    }
    EXPECT_FALSE(ctx, MediaRuntimeNodeFactory::create(*videoSender));

    const MediaNodeId schedulerVideo = graph.addNode(
        MediaNodeKind::DebugDump, "scheduler-video");
    const MediaNodeId schedulerAudio = graph.addNode(
        MediaNodeKind::DebugDump, "scheduler-audio");
    const MediaNodeId scheduler = graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    const MediaNodeId binder = graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    const MediaNodeId schedulerSink = graph.addNode(
        MediaNodeKind::DebugDump, "scheduler-sink");
    graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", runtimePlan.groupKey.value());
    graph.setNodeOption(
        binder, "playback_epoch_binder.sync_group", runtimePlan.groupKey.value());
    graph.addOutputPort(
        schedulerVideo, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(
        schedulerAudio, "packet", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        scheduler, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        scheduler, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(
        scheduler, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        schedulerSink, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    const auto schedulerPolicy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    graph.connect(
        schedulerVideo, "packet", scheduler, "video", "video",
        schedulerPolicy);
    graph.connect(
        schedulerAudio, "packet", scheduler, "audio", "audio",
        schedulerPolicy);
    graph.connect(
        scheduler, "scheduled", schedulerSink, "scheduled", "scheduled",
        schedulerPolicy);
    addPlaybackEpochReleaseBoundary(graph, binder);

    auto clock = std::make_shared<TestMasterClock>(milliseconds(0));
    MediaGraphRuntime runtime(
        std::make_shared<FixedAvSyncClockSource>(clock));
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graph);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        runtimePlan.groupKey, runtimePlan.synchronization,
        runtimePlan.transition});
    EXPECT_TRUE(ctx, runtime.compile(std::move(executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto exactGroup = runtime.context().findAvSyncGroup(runtimePlan.groupKey);
    EXPECT_TRUE(ctx, exactGroup != nullptr);
    EXPECT_TRUE(ctx, dynamic_cast<MediaScheduledRtpSenderNode*>(
                         runtime.scheduler().findNode(
                             built.value().videoSender)) != nullptr);
    EXPECT_TRUE(ctx, dynamic_cast<MediaScheduledRtpSenderNode*>(
                         runtime.scheduler().findNode(
                             built.value().audioSender)) != nullptr);
}

} // namespace

void runScheduledRtpOutputAssemblyTests(TestContext& ctx)
{
    testProductionNodeKinds(ctx);
    testBuilderAndCompilerInjectExactRegisteredGroup(ctx);
}

} // namespace media_transcode::test::scheduled_rtp_output
