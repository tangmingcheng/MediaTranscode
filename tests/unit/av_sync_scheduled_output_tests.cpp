#include "common/GraphRuntimeTestSupport.h"
#include "common/AvSyncRuntimeTestSupport.h"
#include "common/TestAssert.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/time/MediaMappedTimestamp.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

void testSchedulerGenerationPermitClosesUntilExactActivation(
    TestContext& ctx)
{
    MediaProtocolOutputGenerationState state("scheduler_generation_state");
    EXPECT_TRUE(ctx, state.permitActivatedGeneration(1, 0));
    EXPECT_TRUE(ctx, state.validateCommitGeneration(1));
    EXPECT_TRUE(ctx, state.purge(MediaAvGenerationPurge{1, 2, 1}));
    EXPECT_FALSE(ctx, state.validateCommitGeneration(1));
    EXPECT_FALSE(ctx, state.validateCommitGeneration(2));
    EXPECT_FALSE(ctx, state.permitActivatedGeneration(2, 2));
    EXPECT_TRUE(ctx, state.permitActivatedGeneration(2, 1));
    EXPECT_FALSE(ctx, state.validateCommitGeneration(1));
    EXPECT_TRUE(ctx, state.validateCommitGeneration(2));
}

class FixedMasterClock final : public MediaMasterClock {
public:
    explicit FixedMasterClock(MediaRunningTime now) : m_now(now) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(m_now);
    }

private:
    MediaRunningTime m_now;
};

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaRealtimeRtpTranscodeRequest completeRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "task7-scheduled-output";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:5004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    request.input.audioRtp.url = "rtp://127.0.0.1:5006";
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.output.host = "127.0.0.1";
    request.output.basePort = 6000;
    request.output.sdpPath = "task7.sdp";
    request.output.packetSize = 1200;
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'000;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.channels = 2;
    request.parameters.queues.metadata = 4;
    request.parameters.queues.packet = 4;
    request.parameters.queues.frame = 4;
    request.parameters.queues.mux = 4;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    return request;
}

const MediaRealtimeAvSyncRuntimePlan* completeRuntimePlan(
    TestContext& ctx,
    std::optional<MediaRealtimeRtpTranscodePlan>& owner)
{
    auto planned = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, planned);
    if (!planned) return nullptr;
    owner.emplace(std::move(planned).value());
    EXPECT_TRUE(ctx, owner->avSyncRuntime.has_value());
    return owner->avSyncRuntime ? &*owner->avSyncRuntime : nullptr;
}

MediaBufferRef scheduledUnit(
    TestContext& ctx,
    MediaScheduledStream stream,
    std::uint64_t sequence,
    std::int64_t dispatchMs)
{
    const MediaStreamKind streamKind = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    auto packet = makePacketBuffer(
        stream == MediaScheduledStream::Video,
        static_cast<std::int64_t>(sequence), streamKind);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return {};
    auto created = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            std::move(packet).value(), stream, ms(dispatchMs), ms(dispatchMs),
            ms(dispatchMs), ms(dispatchMs), ms(dispatchMs), ms(10), 1,
            MediaSourceAccessUnitSequence(sequence), std::nullopt,
            std::nullopt,
            stream == MediaScheduledStream::Video
                ? std::optional(MediaVideoSyncDecisionKind::Display)
                : std::nullopt});
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : MediaBufferRef{};
}

MediaBufferRef invalidDiscriminatorUnit(TestContext& ctx)
{
    auto packet = makePacketBuffer(false, 1, MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return {};
    auto created = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            std::move(packet).value(), static_cast<MediaScheduledStream>(255),
            ms(1), ms(1), ms(1), ms(1), ms(1), ms(10), 1,
            MediaSourceAccessUnitSequence(1), std::nullopt, std::nullopt,
            std::nullopt});
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : MediaBufferRef{};
}

struct RouterFixture final {
    MediaGraph graph;
    MediaNodeId source;
    MediaNodeId router;
    MediaNodeId videoSink;
    MediaNodeId audioSink;
    MediaGraphExecutionContext execution;
};

RouterFixture routerFixture(
    TestContext& ctx,
    std::size_t videoCapacity = 4,
    std::size_t audioCapacity = 4)
{
    RouterFixture fixture;
    fixture.source = fixture.graph.addNode(MediaNodeKind::DebugDump, "source");
    fixture.router = fixture.graph.addNode(
        MediaNodeKind::ScheduledOutputRouter, "router");
    fixture.graph.setNodeOption(
        fixture.router, "scheduled_output_router.mode", "split_av");
    fixture.videoSink = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "video-sink");
    fixture.audioSink = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "audio-sink");
    fixture.graph.addOutputPort(
        fixture.source, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addInputPort(
        fixture.router, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addOutputPort(
        fixture.router, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addOutputPort(
        fixture.router, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addInputPort(
        fixture.videoSink, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addInputPort(
        fixture.audioSink, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.connect(
        fixture.source, "scheduled", fixture.router, "scheduled", "scheduled",
        MediaBlockingEdgePolicyPlanner::planQueue(8));
    fixture.graph.connect(
        fixture.router, "video", fixture.videoSink, "video", "video",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(videoCapacity));
    fixture.graph.connect(
        fixture.router, "audio", fixture.audioSink, "audio", "audio",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(audioCapacity));
    EXPECT_TRUE(ctx, fixture.execution.compile(fixture.graph));
    return fixture;
}

RouterFixture serializedRouterFixture(TestContext& ctx, std::size_t capacity = 4)
{
    RouterFixture fixture;
    fixture.source = fixture.graph.addNode(MediaNodeKind::DebugDump, "source");
    fixture.router = fixture.graph.addNode(
        MediaNodeKind::ScheduledOutputRouter, "serialized-router");
    fixture.videoSink = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "serialized-sink");
    fixture.graph.setNodeOption(
        fixture.router, "scheduled_output_router.mode", "serialized_av");
    fixture.graph.addOutputPort(
        fixture.source, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addInputPort(
        fixture.router, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addOutputPort(
        fixture.router, "serialized", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.addInputPort(
        fixture.videoSink, "serialized", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture.graph.connect(
        fixture.source, "scheduled", fixture.router, "scheduled", "scheduled",
        MediaBlockingEdgePolicyPlanner::planQueue(8));
    fixture.graph.connect(
        fixture.router, "serialized", fixture.videoSink, "serialized",
        "serialized",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(capacity));
    EXPECT_TRUE(ctx, fixture.execution.compile(fixture.graph));
    return fixture;
}

MediaChannel* routerInput(RouterFixture& fixture)
{
    return fixture.execution.findInputChannel(fixture.router, "scheduled");
}

MediaChannel* videoOutput(RouterFixture& fixture)
{
    return fixture.execution.findInputChannel(fixture.videoSink, "video");
}

MediaChannel* audioOutput(RouterFixture& fixture)
{
    return fixture.execution.findInputChannel(fixture.audioSink, "audio");
}

void expectProcessState(
    TestContext& ctx,
    MediaScheduledOutputRouterNode& router,
    RouterFixture& fixture,
    MediaNodeProcessState expected)
{
    auto result = router.process(fixture.execution);
    EXPECT_TRUE(ctx, result);
    if (result) EXPECT_EQ(ctx, result.value().state, expected);
}

void expectProcessState(
    TestContext& ctx,
    MediaRuntimeNode& node,
    MediaGraphExecutionContext& execution,
    MediaNodeProcessState expected)
{
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result);
    if (result) {
        if (result.value().state != expected) {
            std::cerr << "process state mismatch: actual="
                      << static_cast<int>(result.value().state)
                      << " expected=" << static_cast<int>(expected)
                      << " deadline="
                      << (result.value().deadlineWait ? "yes" : "no") << '\n';
        }
        EXPECT_EQ(ctx, result.value().state, expected);
    }
}

void testSegmentBuildsOneSharedSchedulerAndOneRouter(TestContext& ctx)
{
    std::optional<MediaRealtimeRtpTranscodePlan> owner;
    const auto* plan = completeRuntimePlan(ctx, owner);
    if (!plan) return;
    MediaGraph graph;
    const MediaNodeId video = graph.addNode(MediaNodeKind::DebugDump, "video");
    const MediaNodeId audio = graph.addNode(MediaNodeKind::DebugDump, "audio");
    graph.addOutputPort(
        video, "canonical", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(
        audio, "canonical", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    auto built = MediaRealtimeAvSchedulerSegmentBuilder::build(
        graph,
        MediaRealtimeAvSchedulerSegmentOptions{
            "task7", {video, "canonical"}, {audio, "canonical"}},
        *plan);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    const auto countKind = [&](MediaNodeKind kind) {
        return std::count_if(
            graph.nodes().begin(), graph.nodes().end(),
            [kind](const MediaNode& node) { return node.kind == kind; });
    };
    EXPECT_EQ(ctx, countKind(MediaNodeKind::AvOutputScheduler), 1);
    EXPECT_EQ(ctx, countKind(MediaNodeKind::ScheduledOutputRouter), 1);
    EXPECT_EQ(ctx, built.value().video.port, std::string("video"));
    EXPECT_EQ(ctx, built.value().audio.port, std::string("audio"));
    EXPECT_EQ(ctx, built.value().video.node, built.value().audio.node);
    const MediaNode* scheduler = nullptr;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::AvOutputScheduler) scheduler = &node;
    }
    EXPECT_TRUE(ctx, scheduler != nullptr);
    if (scheduler) {
        EXPECT_EQ(ctx, scheduler->options.value("av_scheduler.sync_group"),
                  plan->groupKey.value());
        const auto& rtp = std::get<MediaSeparateRtpOutputRuntimePlan>(
            plan->protocolOutput);
        EXPECT_EQ(ctx, rtp.video.senderLead, rtp.audio.senderLead);
        EXPECT_EQ(ctx,
                  scheduler->options.value("av_scheduler.transport_lead_ns"),
                  std::to_string(rtp.video.senderLead.nanoseconds()));
    }
    EXPECT_EQ(ctx, graph.edges().size(), static_cast<std::size_t>(3));
    for (const MediaEdge& edge : graph.edges()) {
        EXPECT_EQ(ctx, edge.policy, plan->edgePolicies.synchronizedPacket);
    }

    const auto rejectsInvalidRtpLead = [&](MediaRunningTime videoLead,
                                           MediaRunningTime audioLead) {
        auto candidate = MediaRealtimeRtpTranscodePlanner::plan(
            completeRequest());
        EXPECT_TRUE(ctx, candidate && candidate.value().avSyncRuntime);
        if (!candidate || !candidate.value().avSyncRuntime) return;
        auto& runtimePlan = *candidate.value().avSyncRuntime;
        auto& rtp = std::get<MediaSeparateRtpOutputRuntimePlan>(
            runtimePlan.protocolOutput);
        rtp.video.senderLead = videoLead;
        rtp.audio.senderLead = audioLead;
        MediaGraph invalidGraph;
        const auto invalidVideo = invalidGraph.addNode(
            MediaNodeKind::DebugDump, "invalid-video");
        const auto invalidAudio = invalidGraph.addNode(
            MediaNodeKind::DebugDump, "invalid-audio");
        invalidGraph.addOutputPort(
            invalidVideo, "canonical", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        invalidGraph.addOutputPort(
            invalidAudio, "canonical", MediaStreamKind::Audio,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        EXPECT_FALSE(ctx, MediaRealtimeAvSchedulerSegmentBuilder::build(
            invalidGraph,
            MediaRealtimeAvSchedulerSegmentOptions{
                "invalid-lead", {invalidVideo, "canonical"},
                {invalidAudio, "canonical"}},
            runtimePlan));
    };
    rejectsInvalidRtpLead(ms(0), ms(0));
    rejectsInvalidRtpLead(ms(100), ms(101));

    MediaRealtimeAvSchedulerSegmentOptions missing{
        "task7-missing", {}, {audio, "canonical"}};
    EXPECT_FALSE(ctx,
                 MediaRealtimeAvSchedulerSegmentBuilder::build(
                     graph, missing, *plan));
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::ScheduledOutputRouter), 61);
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(
                         MediaNodeKind::ScheduledOutputRouter));
    MediaNode factoryNode{
        MediaNodeId{999}, MediaNodeKind::ScheduledOutputRouter,
        "factory-router", "factory-router"};
    auto runtime = MediaRuntimeNodeFactory::create(factoryNode);
    EXPECT_TRUE(ctx, runtime);
    if (runtime) {
        EXPECT_TRUE(ctx, dynamic_cast<MediaScheduledOutputRouterNode*>(
                             runtime.value().get()) != nullptr);
    }

    auto consumedPlanned = MediaRealtimeRtpTranscodePlanner::plan(
        completeRequest());
    EXPECT_TRUE(ctx, consumedPlanned &&
                         consumedPlanned.value().avSyncRuntime.has_value());
    if (!consumedPlanned || !consumedPlanned.value().avSyncRuntime) return;
    auto& consumedOnly = *consumedPlanned.value().avSyncRuntime;
    consumedOnly.queues.metadata = 0;
    consumedOnly.queues.frame = 0;
    consumedOnly.queues.mux = 0;
    consumedOnly.edgePolicies.metadata.queuePolicy.capacity = 0;
    consumedOnly.edgePolicies.videoFrame.queuePolicy.capacity = 0;
    consumedOnly.edgePolicies.audioFrame.queuePolicy.capacity = 0;
    consumedOnly.edgePolicies.mux.queuePolicy.capacity = 0;
    MediaGraph consumedGraph;
    const auto consumedVideo = consumedGraph.addNode(
        MediaNodeKind::DebugDump, "consumed-video");
    const auto consumedAudio = consumedGraph.addNode(
        MediaNodeKind::DebugDump, "consumed-audio");
    consumedGraph.addOutputPort(
        consumedVideo, "canonical", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    consumedGraph.addOutputPort(
        consumedAudio, "canonical", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, MediaRealtimeAvSchedulerSegmentBuilder::build(
                         consumedGraph,
                         MediaRealtimeAvSchedulerSegmentOptions{
                             "consumed-only", {consumedVideo, "canonical"},
                             {consumedAudio, "canonical"}},
                         consumedOnly));
}

MediaBufferRef canonicalUnit(
    TestContext& ctx,
    MediaScheduledStream stream,
    std::uint64_t sequence,
    std::int64_t presentationMs)
{
    const MediaStreamKind kind = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    auto packet = makePacketBuffer(
        stream == MediaScheduledStream::Video,
        static_cast<std::int64_t>(sequence), kind);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return {};
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            ms(presentationMs), ms(presentationMs), ms(10),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            stream == MediaScheduledStream::Video
                ? "task7-video"
                : "task7-audio",
            MediaSourceAccessUnitSequence(sequence),
            MediaTimeMappingConfidence::Locked, 1});
    auto created = MediaCanonicalAccessUnitBuffer::create(
        std::move(packet).value(), std::move(lineage),
        stream == MediaScheduledStream::Audio
            ? std::optional<MediaCanonicalAudioSampleInterval>(
                  MediaCanonicalAudioSampleInterval{
                      static_cast<std::int64_t>(sequence - 1) * 480,
                      static_cast<std::int64_t>(sequence) * 480,
                      48'000})
            : std::nullopt);
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : MediaBufferRef{};
}

void testInterleavedUnitsAndIdenticalDispatchEpochsRouteExactlyOnce(
    TestContext& ctx)
{
    auto fixture = routerFixture(ctx);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    const std::vector<MediaBufferRef> units{
        scheduledUnit(ctx, MediaScheduledStream::Video, 1, 100),
        scheduledUnit(ctx, MediaScheduledStream::Audio, 1, 100),
        scheduledUnit(ctx, MediaScheduledStream::Audio, 2, 110),
        scheduledUnit(ctx, MediaScheduledStream::Video, 2, 120)};
    const std::vector<MediaScheduledStream> expected{
        MediaScheduledStream::Video, MediaScheduledStream::Audio,
        MediaScheduledStream::Audio, MediaScheduledStream::Video};
    std::size_t videoCount = 0;
    std::size_t audioCount = 0;
    for (std::size_t index = 0; index < units.size(); ++index) {
        EXPECT_TRUE(ctx, routerInput(fixture)->push(units[index]));
        expectProcessState(ctx, router, fixture, MediaNodeProcessState::Progress);
        MediaBufferRef routed;
        MediaChannel* selected = expected[index] == MediaScheduledStream::Video
            ? videoOutput(fixture)
            : audioOutput(fixture);
        MediaChannel* other = expected[index] == MediaScheduledStream::Video
            ? audioOutput(fixture)
            : videoOutput(fixture);
        EXPECT_TRUE(ctx, selected->tryPop(routed));
        EXPECT_EQ(ctx, routed, units[index]);
        EXPECT_EQ(ctx, other->size(), static_cast<std::size_t>(0));
        if (expected[index] == MediaScheduledStream::Video) ++videoCount;
        else ++audioCount;
    }
    EXPECT_EQ(ctx, videoCount, static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, audioCount, static_cast<std::size_t>(2));
    const auto* video = dynamic_cast<const MediaScheduledAccessUnit*>(
        units[0].get());
    const auto* audio = dynamic_cast<const MediaScheduledAccessUnit*>(
        units[1].get());
    EXPECT_TRUE(ctx, video && audio);
    if (video && audio) {
        EXPECT_EQ(ctx, video->dispatchOnMaster(), audio->dispatchOnMaster());
    }
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testSerializedRouterPreservesGlobalAvOrder(TestContext& ctx)
{
    auto fixture = serializedRouterFixture(ctx);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    const std::vector<MediaBufferRef> units{
        scheduledUnit(ctx, MediaScheduledStream::Video, 1, 100),
        scheduledUnit(ctx, MediaScheduledStream::Audio, 1, 101),
        scheduledUnit(ctx, MediaScheduledStream::Video, 2, 102)};
    auto* input = routerInput(fixture);
    auto* output = fixture.execution.findInputChannel(
        fixture.videoSink, "serialized");
    for (const auto& unit : units) {
        EXPECT_TRUE(ctx, input->push(unit));
        expectProcessState(ctx, router, fixture, MediaNodeProcessState::Progress);
    }
    for (const auto& expected : units) {
        MediaBufferRef actual;
        EXPECT_TRUE(ctx, output->tryPop(actual));
        EXPECT_EQ(ctx, actual, expected);
    }
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testBlockedMediaOutputRetriesWithoutDuplicateDropOrCrossRoute(
    TestContext& ctx,
    MediaScheduledStream blockedStream)
{
    auto fixture = routerFixture(ctx, 1, 1);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    MediaChannel* selected = blockedStream == MediaScheduledStream::Video
        ? videoOutput(fixture)
        : audioOutput(fixture);
    MediaChannel* other = blockedStream == MediaScheduledStream::Video
        ? audioOutput(fixture)
        : videoOutput(fixture);
    auto filler = scheduledUnit(ctx, blockedStream, 1, 1);
    auto unit = scheduledUnit(ctx, blockedStream, 2, 2);
    EXPECT_TRUE(ctx, selected->push(filler));
    EXPECT_TRUE(ctx, routerInput(fixture)->push(unit));
    expectProcessState(ctx, router, fixture, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, selected->size(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, other->size(), static_cast<std::size_t>(0));
    MediaBufferRef popped;
    EXPECT_TRUE(ctx, selected->tryPop(popped));
    EXPECT_EQ(ctx, popped, filler);
    expectProcessState(ctx, router, fixture, MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, selected->tryPop(popped));
    EXPECT_EQ(ctx, popped, unit);
    expectProcessState(ctx, router, fixture, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, selected->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, other->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testInvalidDiscriminatorFailsClosed(TestContext& ctx)
{
    auto fixture = routerFixture(ctx);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    EXPECT_TRUE(ctx, routerInput(fixture)->push(invalidDiscriminatorUnit(ctx)));
    auto result = router.process(fixture.execution);
    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_EQ(ctx, result.error().code, ::media::ErrorCode::InvalidArgument);
    }
    EXPECT_EQ(ctx, videoOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audioOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testTypedControlFansOutAtomicallyAfterMedia(TestContext& ctx)
{
    for (const auto kind : {
             MediaControlBufferKind::Eof,
             MediaControlBufferKind::Flush,
             MediaControlBufferKind::Abort}) {
        auto fixture = routerFixture(ctx, 3, 3);
        MediaScheduledOutputRouterNode router(fixture.router);
        EXPECT_TRUE(ctx, router.start(fixture.execution));
        auto video = scheduledUnit(ctx, MediaScheduledStream::Video, 1, 1);
        auto audio = scheduledUnit(ctx, MediaScheduledStream::Audio, 1, 2);
        auto terminal = makeMediaBufferRef<MediaControlBuffer>(kind);
        EXPECT_TRUE(ctx, routerInput(fixture)->push(video));
        EXPECT_TRUE(ctx, routerInput(fixture)->push(audio));
        EXPECT_TRUE(ctx, routerInput(fixture)->push(terminal));
        expectProcessState(ctx, router, fixture, MediaNodeProcessState::Progress);
        expectProcessState(ctx, router, fixture, MediaNodeProcessState::Progress);
        expectProcessState(
            ctx, router, fixture, MediaNodeProcessState::Finished);
        MediaBufferRef value;
        EXPECT_TRUE(ctx, videoOutput(fixture)->tryPop(value));
        EXPECT_EQ(ctx, value, video);
        EXPECT_TRUE(ctx, videoOutput(fixture)->tryPop(value));
        EXPECT_EQ(ctx, value, terminal);
        EXPECT_TRUE(ctx, audioOutput(fixture)->tryPop(value));
        EXPECT_EQ(ctx, value, audio);
        EXPECT_TRUE(ctx, audioOutput(fixture)->tryPop(value));
        EXPECT_EQ(ctx, value, terminal);
        EXPECT_TRUE(ctx, router.stop(fixture.execution));
    }
}

void testFlushFinishesBeforeSchedulerOutputCloses(TestContext& ctx)
{
    auto fixture = routerFixture(ctx, 1, 1);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    auto flush = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    EXPECT_TRUE(ctx, routerInput(fixture)->push(flush));
    expectProcessState(
        ctx, router, fixture, MediaNodeProcessState::Finished);

    routerInput(fixture)->close();
    expectProcessState(
        ctx, router, fixture, MediaNodeProcessState::Finished);

    MediaBufferRef value;
    EXPECT_TRUE(ctx, videoOutput(fixture)->tryPop(value));
    EXPECT_EQ(ctx, value, flush);
    EXPECT_TRUE(ctx, audioOutput(fixture)->tryPop(value));
    EXPECT_EQ(ctx, value, flush);
    EXPECT_EQ(ctx, videoOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audioOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testBlockedTerminalPreflightsBothOutputsBeforeRetry(TestContext& ctx)
{
    auto fixture = routerFixture(ctx, 1, 1);
    MediaScheduledOutputRouterNode router(fixture.router);
    EXPECT_TRUE(ctx, router.start(fixture.execution));
    auto filler = scheduledUnit(ctx, MediaScheduledStream::Video, 1, 1);
    auto eof = makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Eof);
    EXPECT_TRUE(ctx, videoOutput(fixture)->push(filler));
    EXPECT_TRUE(ctx, routerInput(fixture)->push(eof));
    expectProcessState(ctx, router, fixture, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, videoOutput(fixture)->size(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, audioOutput(fixture)->size(), static_cast<std::size_t>(0));
    MediaBufferRef value;
    EXPECT_TRUE(ctx, videoOutput(fixture)->tryPop(value));
    EXPECT_EQ(ctx, value, filler);
    expectProcessState(ctx, router, fixture, MediaNodeProcessState::Finished);
    EXPECT_TRUE(ctx, videoOutput(fixture)->tryPop(value));
    EXPECT_EQ(ctx, value, eof);
    EXPECT_TRUE(ctx, audioOutput(fixture)->tryPop(value));
    EXPECT_EQ(ctx, value, eof);
    EXPECT_EQ(ctx, videoOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audioOutput(fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router.stop(fixture.execution));
}

void testActiveSharedSchedulerRoutesEqualEpochWithoutDuplicateRetry(
    TestContext& ctx)
{
    std::optional<MediaRealtimeRtpTranscodePlan> owner;
    const auto* plan = completeRuntimePlan(ctx, owner);
    if (!plan) return;
    MediaGraph graph;
    const MediaNodeId video = graph.addNode(MediaNodeKind::DebugDump, "video");
    const MediaNodeId audio = graph.addNode(MediaNodeKind::DebugDump, "audio");
    const MediaNodeId binder = graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    graph.setNodeOption(
        binder, "playback_epoch_binder.sync_group", plan->groupKey.value());
    graph.addOutputPort(
        video, "canonical", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(
        audio, "canonical", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    auto built = MediaRealtimeAvSchedulerSegmentBuilder::build(
        graph,
        MediaRealtimeAvSchedulerSegmentOptions{
            "active", {video, "canonical"}, {audio, "canonical"}},
        *plan);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    const MediaNodeId videoSink = graph.addNode(MediaNodeKind::DebugDump, "vs");
    const MediaNodeId audioSink = graph.addNode(MediaNodeKind::DebugDump, "as");
    graph.addInputPort(
        videoSink, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(
        audioSink, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.connect(
        built.value().video.node, built.value().video.port,
        videoSink, "video", "video sink",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(1));
    graph.connect(
        built.value().audio.node, built.value().audio.port,
        audioSink, "audio", "audio sink",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(1));
    MediaGraphExecutionContext execution;
    std::unique_ptr<MediaGraphRuntime> runtime;
    EXPECT_TRUE(ctx, media_transcode::test::compileAndActivateAvSyncRuntime(
                         std::move(graph),
                          {plan->groupKey, plan->synchronization,
                           media_transcode::test::
                               schedulerOnlyComponentTransitionPlan(
                                   plan->transition.acknowledgementTimeout,
                                   plan->transition.terminalDrainWindow),
                           MediaAvSyncBindingAssemblyMode::ComponentCore},
                         std::make_shared<FixedMasterClock>(ms(0)),
                         MediaPlaybackEpoch{ms(0), ms(0), 1}, binder,
                         execution, runtime));
    if (!runtime) return;
    const auto schedulerIt = std::find_if(
        runtime->graph()->nodes().begin(), runtime->graph()->nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::AvOutputScheduler;
        });
    const auto routerIt = std::find_if(
        runtime->graph()->nodes().begin(), runtime->graph()->nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::ScheduledOutputRouter;
        });
    EXPECT_TRUE(ctx, schedulerIt != runtime->graph()->nodes().end());
    EXPECT_TRUE(ctx, routerIt != runtime->graph()->nodes().end());
    if (schedulerIt == runtime->graph()->nodes().end() ||
        routerIt == runtime->graph()->nodes().end()) return;
    auto* scheduler = dynamic_cast<MediaAvOutputSchedulerNode*>(
        runtime->scheduler().findNode(schedulerIt->id));
    auto* router = dynamic_cast<MediaScheduledOutputRouterNode*>(
        runtime->scheduler().findNode(routerIt->id));
    EXPECT_TRUE(ctx, scheduler && router);
    if (!scheduler || !router) return;
    EXPECT_TRUE(ctx, scheduler->start(execution));
    EXPECT_TRUE(ctx, router->start(execution));
    auto activeGroup = execution.findAvSyncGroup(plan->groupKey);
    EXPECT_TRUE(ctx, activeGroup && activeGroup->lifecycleState() ==
                         MediaAvSyncGroupRuntime::LifecycleState::Active);

    const auto canonicalVideo = canonicalUnit(
        ctx, MediaScheduledStream::Video, 1, 0);
    const auto canonicalAudio = canonicalUnit(
        ctx, MediaScheduledStream::Audio, 1, 0);
    EXPECT_TRUE(ctx, execution.findInputChannel(schedulerIt->id, "video")
                         ->push(canonicalVideo));
    EXPECT_TRUE(ctx, execution.findInputChannel(schedulerIt->id, "audio")
                         ->push(canonicalAudio));
    execution.findInputChannel(schedulerIt->id, "video")->close();
    execution.findInputChannel(schedulerIt->id, "audio")->close();
    expectProcessState(
        ctx, *scheduler, execution, MediaNodeProcessState::Progress);
    expectProcessState(
        ctx, *router, execution, MediaNodeProcessState::Progress);
    MediaBufferRef routedAudio;
    EXPECT_TRUE(ctx, execution.findInputChannel(audioSink, "audio")
                         ->tryPop(routedAudio));
    const auto* scheduledAudio = dynamic_cast<const MediaScheduledAccessUnit*>(
        routedAudio.get());
    EXPECT_TRUE(ctx, scheduledAudio != nullptr);

    auto videoFiller = scheduledUnit(
        ctx, MediaScheduledStream::Video, 99, 0);
    MediaChannel* videoResult = execution.findInputChannel(videoSink, "video");
    EXPECT_TRUE(ctx, videoResult->push(videoFiller));
    expectProcessState(
        ctx, *scheduler, execution, MediaNodeProcessState::Progress);
    expectProcessState(
        ctx, *router, execution, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, videoResult->size(), static_cast<std::size_t>(1));
    MediaBufferRef value;
    EXPECT_TRUE(ctx, videoResult->tryPop(value));
    EXPECT_EQ(ctx, value, videoFiller);
    expectProcessState(
        ctx, *router, execution, MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, videoResult->tryPop(value));
    const auto* scheduledVideo = dynamic_cast<const MediaScheduledAccessUnit*>(
        value.get());
    EXPECT_TRUE(ctx, scheduledVideo != nullptr);
    if (scheduledAudio && scheduledVideo) {
        EXPECT_EQ(ctx, scheduledAudio->dispatchOnMaster(),
                  scheduledVideo->dispatchOnMaster());
        EXPECT_EQ(ctx, scheduledAudio->generation(),
                  scheduledVideo->generation());
    }
    expectProcessState(
        ctx, *router, execution, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, videoResult->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, execution.findInputChannel(audioSink, "audio")->size(),
              static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, router->stop(execution));
    EXPECT_TRUE(ctx, scheduler->stop(execution));
}

} // namespace

int main()
{
    TestContext ctx;
    testSchedulerGenerationPermitClosesUntilExactActivation(ctx);
    testSegmentBuildsOneSharedSchedulerAndOneRouter(ctx);
    testInterleavedUnitsAndIdenticalDispatchEpochsRouteExactlyOnce(ctx);
    testSerializedRouterPreservesGlobalAvOrder(ctx);
    testBlockedMediaOutputRetriesWithoutDuplicateDropOrCrossRoute(
        ctx, MediaScheduledStream::Video);
    testBlockedMediaOutputRetriesWithoutDuplicateDropOrCrossRoute(
        ctx, MediaScheduledStream::Audio);
    testInvalidDiscriminatorFailsClosed(ctx);
    testTypedControlFansOutAtomicallyAfterMedia(ctx);
    testFlushFinishesBeforeSchedulerOutputCloses(ctx);
    testBlockedTerminalPreflightsBothOutputsBeforeRetry(ctx);
    testActiveSharedSchedulerRoutesEqualEpochWithoutDuplicateRetry(ctx);
    if (ctx.failures != 0) return 1;
    std::cout << "A/V scheduled output tests passed\n";
    return 0;
}
