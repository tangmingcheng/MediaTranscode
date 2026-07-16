#include "common/TestAssert.h"

#include "internal/graph/nodes/sync/MediaAvStartupClockNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochActivatedFanoutNode.h"
#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <chrono>
#include <atomic>
#include <memory>
#include <type_traits>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

static_assert(std::is_final_v<MediaPlaybackEpochActivatedBuffer>);
static_assert(std::is_final_v<MediaPlaybackEpochActivatedFanoutNode>);
static_assert(std::is_final_v<MediaRtpSourceClockStateAdapterNode>);
static_assert(std::is_final_v<MediaAvStartupClockNode>);

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaPlaybackEpoch epoch(std::uint64_t generation = 1)
{
    return {ms(10), ms(20), generation};
}

MediaAudioPlaybackOrigin origin(std::uint64_t generation = 1)
{
    return {generation, ms(10), ms(20), 0, 48'000};
}

MediaAvSyncPlan completePlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "task4-release";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    auto planned = MediaAvSyncPlanner::plan(request);
    auto plan = std::move(planned).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    return plan;
}

MediaAvGenerationTransitionPlan transitionPlan()
{
    return MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp, ms(1'000), ms(500));
}

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(MediaRunningTime now) : m_now(now.nanoseconds()) {}
    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(m_now.load()));
    }
    void set(MediaRunningTime now) noexcept { m_now = now.nanoseconds(); }

private:
    std::atomic<std::int64_t> m_now;
};

class TestClockSource final : public MediaAvSyncClockSource {
public:
    explicit TestClockSource(std::shared_ptr<TestMasterClock> clock)
        : m_clock(std::move(clock)) {}
    ::media::Result<MediaAvSyncClockBundle> capture(bool requireNtp) override
    {
        std::shared_ptr<const MediaSharedNtpEpoch> ntp;
        if (requireNtp) {
            auto created = MediaSharedNtpEpoch::create(
                ms(0), std::chrono::nanoseconds(0));
            if (!created) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    created.error());
            }
            ntp = std::make_shared<const MediaSharedNtpEpoch>(
                std::move(created).value());
        }
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{m_clock, std::move(ntp)});
    }

private:
    std::shared_ptr<TestMasterClock> m_clock;
};

struct BinderFixture final {
    MediaRealtimeExecutableGraph executable;
    MediaNodeId binder;
    MediaNodeId source;
    MediaNodeId activatedSink;
    MediaNodeId releaseSink;
};

BinderFixture binderFixture()
{
    BinderFixture fixture;
    const auto scheduler = fixture.executable.graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    fixture.binder = fixture.executable.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    fixture.source = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "source");
    fixture.activatedSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "activated-sink");
    fixture.releaseSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "release-sink");
    fixture.executable.graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.binder, "playback_epoch_binder.sync_group", "task4-group");
    fixture.executable.graph.addOutputPort(
        fixture.source, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.binder, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        fixture.binder, "activated", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        fixture.binder, "bound_release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.activatedSink, "in", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.releaseSink, "in", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto one = MediaGraphBuildSupport::blockingQueuePolicy(1);
    fixture.executable.graph.connect(
        fixture.source, "release", fixture.binder, "release", "release", one);
    fixture.executable.graph.connect(
        fixture.binder, "activated", fixture.activatedSink, "in", "activated", one);
    fixture.executable.graph.connect(
        fixture.binder, "bound_release", fixture.releaseSink, "in", "bound", one);
    fixture.executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("task4-group"), completePlan(), transitionPlan()});
    return fixture;
}

MediaBufferRef release(TestContext& ctx)
{
    const auto payload = [] {
        return makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(1));
    };
    auto created = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("task4-group"),
        MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch(), origin(), {{payload(), 0}}, {{payload(), 0}});
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : MediaBufferRef{};
}

void testTaskFourRuntimeKindsAreAppendOnly(TestContext& ctx)
{
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::PlaybackEpochActivatedFanout), 54);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::RtpSourceClockStateAdapter), 55);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::AvStartupClock), 56);
    EXPECT_EQ(ctx, MediaPlaybackEpochActivatedFanoutNode::staticKind(),
              MediaNodeKind::PlaybackEpochActivatedFanout);
    EXPECT_EQ(ctx, MediaRtpSourceClockStateAdapterNode::staticKind(),
              MediaNodeKind::RtpSourceClockStateAdapter);
    EXPECT_EQ(ctx, MediaAvStartupClockNode::staticKind(),
              MediaNodeKind::AvStartupClock);
}

void testActivatedEventIsCompleteAndImmutable(TestContext& ctx)
{
    auto created = MediaPlaybackEpochActivatedBuffer::create(
        MediaAvSyncGroupKey("task4-group"), epoch(), origin());
    EXPECT_TRUE(ctx, created);
    const auto* event = created
        ? dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(created.value().get())
        : nullptr;
    EXPECT_TRUE(ctx, event != nullptr);
    if (event) {
        EXPECT_EQ(ctx, event->groupKey(), MediaAvSyncGroupKey("task4-group"));
        EXPECT_EQ(ctx, event->epoch(), epoch());
        EXPECT_EQ(ctx, event->audioOrigin(), origin());
    }
    EXPECT_FALSE(ctx, MediaPlaybackEpochActivatedBuffer::create(
                          MediaAvSyncGroupKey(""), epoch(), origin()));
    EXPECT_FALSE(ctx, MediaPlaybackEpochActivatedBuffer::create(
                          MediaAvSyncGroupKey("task4-group"), epoch(2), origin(1)));
}

void testBinderActivatesOnceAndCommitsEventBeforeRelease(TestContext& ctx)
{
    auto fixture = binderFixture();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime.scheduler().findNode(fixture.binder));
    auto group = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("task4-group"));
    auto* input = runtime.context().findInputChannel(fixture.binder, "release");
    auto* activated = runtime.context().findInputChannel(fixture.activatedSink, "in");
    auto* bound = runtime.context().findInputChannel(fixture.releaseSink, "in");
    EXPECT_TRUE(ctx, binder && group && input && activated && bound);
    if (!binder || !group || !input || !activated || !bound) return;

    auto eventFiller = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0));
    auto releaseFiller = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0));
    EXPECT_TRUE(ctx, activated->push(eventFiller));
    EXPECT_TRUE(ctx, bound->push(releaseFiller));
    auto expectedRelease = release(ctx);
    EXPECT_TRUE(ctx, input->push(expectedRelease));

    auto first = binder->process(runtime.context());
    EXPECT_TRUE(ctx, first && first.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Active);
    auto second = binder->process(runtime.context());
    EXPECT_TRUE(ctx, second && second.value().state == MediaNodeProcessState::Waiting);

    MediaBufferRef popped;
    EXPECT_TRUE(ctx, activated->tryPop(popped));
    EXPECT_TRUE(ctx, binder->process(runtime.context()));
    EXPECT_EQ(ctx, activated->size(), static_cast<std::size_t>(1));
    MediaBufferRef activatedEvent;
    EXPECT_TRUE(ctx, activated->tryPop(activatedEvent));
    EXPECT_TRUE(ctx, dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
                         activatedEvent.get()) != nullptr);
    auto releaseStillBlocked = binder->process(runtime.context());
    EXPECT_TRUE(ctx, releaseStillBlocked &&
                         releaseStillBlocked.value().state ==
                             MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, activated->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, bound->tryPop(popped));
    EXPECT_TRUE(ctx, binder->process(runtime.context()));
    MediaBufferRef forwarded;
    EXPECT_TRUE(ctx, bound->tryPop(forwarded));
    EXPECT_TRUE(ctx, forwarded == expectedRelease);
    auto idle = binder->process(runtime.context());
    EXPECT_TRUE(ctx, idle && idle.value().state == MediaNodeProcessState::Waiting);
}

void testActivatedFanoutPreservesOneImmutableEvent(TestContext& ctx)
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto fanout = graph.addNode(
        MediaNodeKind::PlaybackEpochActivatedFanout, "fanout");
    const auto firstSink = graph.addNode(MediaNodeKind::DebugDump, "first");
    const auto secondSink = graph.addNode(MediaNodeKind::DebugDump, "second");
    graph.addOutputPort(source, "activated", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(fanout, "activated", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(fanout, "events", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent,
                        true, true);
    graph.addInputPort(firstSink, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(secondSink, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    graph.connect(source, "activated", fanout, "activated", "input", policy);
    graph.connect(fanout, "events", firstSink, "in", "first", policy);
    graph.connect(fanout, "events", secondSink, "in", "second", policy);
    MediaGraphExecutionContext execution;
    const auto compiled = execution.compile(graph);
    EXPECT_TRUE(ctx, compiled);
    if (!compiled) return;
    MediaPlaybackEpochActivatedFanoutNode node(fanout);
    auto event = MediaPlaybackEpochActivatedBuffer::create(
        MediaAvSyncGroupKey("task4-group"), epoch(), origin());
    EXPECT_TRUE(ctx, event);
    EXPECT_TRUE(ctx, execution.findInputChannel(fanout, "activated")->push(
        event.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef first;
    MediaBufferRef second;
    EXPECT_TRUE(ctx, execution.findInputChannel(firstSink, "in")->tryPop(first));
    EXPECT_TRUE(ctx, execution.findInputChannel(secondSink, "in")->tryPop(second));
    EXPECT_TRUE(ctx, first == event.value() && second == event.value());
}

void testTaskFourFactorySurfaceIsComplete(TestContext& ctx)
{
    for (const auto kind : {
             MediaNodeKind::PlaybackEpochActivatedFanout,
             MediaNodeKind::RtpSourceClockStateAdapter,
             MediaNodeKind::AvStartupClock}) {
        EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(kind));
        MediaNode model{MediaNodeId{900 + static_cast<std::uint64_t>(kind)},
                        kind, "task4.factory"};
        EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::create(model));
    }
}

void testBinderAbortDropsRetainedRelease(TestContext& ctx)
{
    auto fixture = binderFixture();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime.scheduler().findNode(fixture.binder));
    auto* input = runtime.context().findInputChannel(fixture.binder, "release");
    auto* activated = runtime.context().findInputChannel(fixture.activatedSink, "in");
    auto* bound = runtime.context().findInputChannel(fixture.releaseSink, "in");
    EXPECT_TRUE(ctx, binder && input && activated && bound);
    if (!binder || !input || !activated || !bound) return;
    EXPECT_TRUE(ctx, activated->push(
        makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0))));
    EXPECT_TRUE(ctx, input->push(release(ctx)));
    EXPECT_TRUE(ctx, binder->process(runtime.context()));
    binder->abort(runtime.context());
    MediaBufferRef ignored;
    EXPECT_TRUE(ctx, activated->tryPop(ignored));
    auto afterAbort = binder->process(runtime.context());
    EXPECT_TRUE(ctx, afterAbort &&
                         afterAbort.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, bound->size(), static_cast<std::size_t>(0));
}

void testRtpAdapterPublishesGenericLockedState(TestContext& ctx)
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto adapter = graph.addNode(
        MediaNodeKind::RtpSourceClockStateAdapter, "adapter");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
    graph.addOutputPort(source, "clock", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(adapter, "clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(adapter, "state", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(sink, "state", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    graph.connect(source, "clock", adapter, "clock", "clock", policy);
    graph.connect(adapter, "state", sink, "state", "state", policy);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaRtpSourceClockStateAdapterNode node(adapter);
    MediaRtpSourceClockCalibration calibration{
        7, {'c'}, 1, 100, 100, ms(10), ms(10), 1'000'000'000, 90'000,
        0, MediaRtpSourceClockConfidence::Locked};
    MediaRtpLockedClockGroup locked{
        ms(10), {'c'}, calibration, calibration};
    auto snapshot = makeMediaBufferRef<MediaRtpClockGroupBuffer>(
        MediaRtpClockGroupSnapshot{
            MediaRtpClockGroupState::Locked, 1, std::move(locked)});
    EXPECT_TRUE(ctx, execution.findInputChannel(adapter, "clock")->push(snapshot));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef output;
    EXPECT_TRUE(ctx, execution.findInputChannel(sink, "state")->tryPop(output));
    const auto* state = dynamic_cast<const MediaSourceClockStateBuffer*>(
        output.get());
    EXPECT_TRUE(ctx, state &&
                         state->readiness() == MediaSourceClockReadiness::Locked &&
                         state->generation() == 1);
}

void testStartupClockUsesRegisteredMasterDeadline(TestContext& ctx)
{
    auto fixture = binderFixture();
    const auto source = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "clock-state-source");
    const auto clockNode = fixture.executable.graph.addNode(
        MediaNodeKind::AvStartupClock, "startup-clock");
    const auto sink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "tick-sink");
    fixture.executable.graph.setNodeOption(
        clockNode, "av_startup_clock.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        clockNode, "av_startup_clock.interval_ns", "10000000");
    fixture.executable.graph.addOutputPort(
        source, "state", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        clockNode, "clock", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        clockNode, "tick", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        sink, "tick", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    fixture.executable.graph.connect(
        source, "state", clockNode, "clock", "state", policy);
    fixture.executable.graph.connect(
        clockNode, "tick", sink, "tick", "tick", policy);
    auto master = std::make_shared<TestMasterClock>(ms(100));
    MediaGraphRuntime runtime(std::make_shared<TestClockSource>(master));
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto* node = dynamic_cast<MediaAvStartupClockNode*>(
        runtime.scheduler().findNode(clockNode));
    EXPECT_TRUE(ctx, node != nullptr);
    if (!node) return;
    EXPECT_TRUE(ctx, node->start(runtime.context()));
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(clockNode, "clock")->push(
        makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::Locked, 1, false)));
    EXPECT_TRUE(ctx, node->process(runtime.context()));
    MediaBufferRef first;
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(first));
    const auto* tick = dynamic_cast<const MediaAvStartupClockBuffer*>(first.get());
    EXPECT_TRUE(ctx, tick && tick->masterNow() == ms(100));
    auto waiting = node->process(runtime.context());
    EXPECT_TRUE(ctx, waiting && waiting.value().deadlineWait &&
                         waiting.value().deadlineWait->syncGroup ==
                             MediaAvSyncGroupKey("task4-group") &&
                         waiting.value().deadlineWait->masterDeadline == ms(110));
    master->set(ms(110));
    EXPECT_TRUE(ctx, node->process(runtime.context()));
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->size() == 1);
    EXPECT_TRUE(ctx, node->stop(runtime.context()));
}

} // namespace

int main()
{
    TestContext ctx;
    testTaskFourRuntimeKindsAreAppendOnly(ctx);
    testActivatedEventIsCompleteAndImmutable(ctx);
    testBinderActivatesOnceAndCommitsEventBeforeRelease(ctx);
    testBinderAbortDropsRetainedRelease(ctx);
    testActivatedFanoutPreservesOneImmutableEvent(ctx);
    testTaskFourFactorySurfaceIsComplete(ctx);
    testRtpAdapterPublishesGenericLockedState(ctx);
    testStartupClockUsesRegisteredMasterDeadline(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
