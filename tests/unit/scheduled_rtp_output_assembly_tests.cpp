#include "unit/fixtures/ScheduledRtpOutputNodeTestSupport.h"
#include "unit/fixtures/ScheduledRtpOutputIntegrationGraph.h"

#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include <memory>
#include <algorithm>
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

void testIntegrationGraphUsesSingleProductionSchedulingPath(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto built = ScheduledRtpOutputIntegrationGraphBuilder::build(
        *outer.value().avSyncRuntime);
    EXPECT_TRUE(ctx, built);
    if (!built) return;

    const auto& fixture = built.value();
    const auto countKind = [&](MediaNodeKind kind) {
        return std::count_if(
            fixture.graph.nodes().begin(), fixture.graph.nodes().end(),
            [&](const MediaNode& node) { return node.kind == kind; });
    };
    EXPECT_EQ(ctx, countKind(MediaNodeKind::AvOutputScheduler), 1u);
    EXPECT_EQ(ctx, countKind(MediaNodeKind::ScheduledOutputRouter), 1u);

    const MediaPort* routerVideo =
        fixture.graph.findOutputPort(fixture.router, "video");
    const MediaPort* routerAudio =
        fixture.graph.findOutputPort(fixture.router, "audio");
    const MediaPort* videoScheduled =
        fixture.graph.findInputPort(fixture.videoSender, "scheduled");
    const MediaPort* audioScheduled =
        fixture.graph.findInputPort(fixture.audioSender, "scheduled");
    EXPECT_TRUE(ctx, routerVideo && routerAudio && videoScheduled &&
                         audioScheduled);
    if (!routerVideo || !routerAudio || !videoScheduled || !audioScheduled) {
        return;
    }
    const auto countExactEdge = [&](MediaPortId from, MediaPortId to) {
        return std::count_if(
            fixture.graph.edges().begin(), fixture.graph.edges().end(),
            [&](const MediaEdge& edge) {
                return edge.from.portId == from && edge.to.portId == to;
            });
    };
    const auto countIncoming = [&](MediaPortId to) {
        return std::count_if(
            fixture.graph.edges().begin(), fixture.graph.edges().end(),
            [&](const MediaEdge& edge) { return edge.to.portId == to; });
    };
    EXPECT_EQ(ctx, countExactEdge(routerVideo->id, videoScheduled->id), 1u);
    EXPECT_EQ(ctx, countExactEdge(routerAudio->id, audioScheduled->id), 1u);
    EXPECT_EQ(ctx, countIncoming(videoScheduled->id), 1u);
    EXPECT_EQ(ctx, countIncoming(audioScheduled->id), 1u);
}

void testBuilderAndCompilerInjectExactRegisteredGroup(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto runtimePlan = std::move(*outer.value().avSyncRuntime);
    auto built = ScheduledRtpOutputIntegrationGraphBuilder::build(runtimePlan);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    auto& fixture = built.value();
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(fixture.graph).ok());
    const MediaNode* videoSender =
        fixture.graph.findNode(fixture.videoSender);
    const MediaNode* audioSender =
        fixture.graph.findNode(fixture.audioSender);
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

    auto clock = std::make_shared<TestMasterClock>(milliseconds(0));
    MediaGraphRuntime runtime(
        std::make_shared<FixedAvSyncClockSource>(clock));
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(fixture.graph);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        runtimePlan.groupKey, runtimePlan.synchronization,
        runtimePlan.transition,
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_TRUE(ctx, runtime.compile(std::move(executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto exactGroup = runtime.context().findAvSyncGroup(runtimePlan.groupKey);
    EXPECT_TRUE(ctx, exactGroup != nullptr);
    EXPECT_TRUE(ctx, dynamic_cast<MediaScheduledRtpSenderNode*>(
                         runtime.scheduler().findNode(
                             fixture.videoSender)) != nullptr);
    EXPECT_TRUE(ctx, dynamic_cast<MediaScheduledRtpSenderNode*>(
                         runtime.scheduler().findNode(
                             fixture.audioSender)) != nullptr);
}

} // namespace

void runScheduledRtpOutputAssemblyTests(TestContext& ctx)
{
    testProductionNodeKinds(ctx);
    testIntegrationGraphUsesSingleProductionSchedulingPath(ctx);
    testBuilderAndCompilerInjectExactRegisteredGroup(ctx);
}

} // namespace media_transcode::test::scheduled_rtp_output
