#include "common/TestAssert.h"

#include "internal/graph/nodes/sync/MediaAvStartupClockNode.h"
#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"
#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
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
static_assert(std::is_final_v<MediaStartupReleaseTransactionBuffer>);
static_assert(std::is_final_v<MediaActivatedStartupReleaseSequencerNode>);
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
    MediaNodeId sequencer;
    MediaNodeId firstActivatedSink;
    MediaNodeId secondActivatedSink;
    MediaNodeId releaseSink;
};

class WaitingNode final : public MediaRuntimeNode {
public:
    explicit WaitingNode(MediaNodeId id) : m_id(id) {}
    MediaNodeId nodeId() const noexcept override { return m_id; }
    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext&) override
    {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }

private:
    MediaNodeId m_id;
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
    fixture.sequencer = fixture.executable.graph.addNode(
        MediaNodeKind::ActivatedStartupReleaseSequencer, "sequencer");
    fixture.firstActivatedSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "first-activated-sink");
    fixture.secondActivatedSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "second-activated-sink");
    fixture.releaseSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "release-sink");
    fixture.executable.graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.binder, "playback_epoch_binder.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.sequencer,
        "activated_startup_release_sequencer.sync_group", "task4-group");
    fixture.executable.graph.addOutputPort(
        fixture.source, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.binder, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        fixture.binder, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.sequencer, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        fixture.sequencer, "activated", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    fixture.executable.graph.addOutputPort(
        fixture.sequencer, "bound_release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.firstActivatedSink, "in", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.secondActivatedSink, "in", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.releaseSink, "in", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto one = MediaGraphBuildSupport::blockingQueuePolicy(1);
    fixture.executable.graph.connect(
        fixture.source, "release", fixture.binder, "release", "release", one);
    fixture.executable.graph.connect(
        fixture.binder, "transaction", fixture.sequencer, "transaction",
        "transaction", one);
    fixture.executable.graph.connect(
        fixture.sequencer, "activated", fixture.firstActivatedSink, "in",
        "first-activated", one);
    fixture.executable.graph.connect(
        fixture.sequencer, "activated", fixture.secondActivatedSink, "in",
        "second-activated", one);
    fixture.executable.graph.connect(
        fixture.sequencer, "bound_release", fixture.releaseSink, "in", "bound", one);
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
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::ReservedNodeKind54), 54);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::RtpSourceClockStateAdapter), 55);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::AvStartupClock), 56);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::ActivatedStartupReleaseSequencer), 57);
    EXPECT_EQ(ctx, MediaActivatedStartupReleaseSequencerNode::staticKind(),
              MediaNodeKind::ActivatedStartupReleaseSequencer);
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

void testActivationReleaseTransactionPreservesReferences(TestContext& ctx)
{
    auto expectedRelease = release(ctx);
    auto envelope = MediaStartupReleaseTransactionBuffer::create(
        expectedRelease);
    EXPECT_TRUE(ctx, envelope);
    const auto* transaction = envelope
        ? dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
              envelope.value().get())
        : nullptr;
    EXPECT_TRUE(ctx, transaction != nullptr);
    if (transaction) {
        EXPECT_TRUE(ctx, transaction->release() == expectedRelease);
        EXPECT_EQ(ctx, transaction->groupKey(),
                  MediaAvSyncGroupKey("task4-group"));
        EXPECT_EQ(ctx, transaction->epoch(), epoch());
    }
}

struct RuntimeFixture final {
    BinderFixture model;
    MediaGraphRuntime runtime;
    MediaPlaybackEpochBinderNode* binder = nullptr;
    MediaActivatedStartupReleaseSequencerNode* sequencer = nullptr;
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
    MediaChannel* releaseInput = nullptr;
    MediaChannel* transaction = nullptr;
    MediaChannel* firstEvent = nullptr;
    MediaChannel* secondEvent = nullptr;
    MediaChannel* boundRelease = nullptr;
};

std::unique_ptr<RuntimeFixture> startFixture(TestContext& ctx)
{
    auto fixture = std::make_unique<RuntimeFixture>();
    fixture->model = binderFixture();
    EXPECT_TRUE(ctx, fixture->runtime.compile(std::move(fixture->model.executable)));
    EXPECT_TRUE(ctx, fixture->runtime.registerDefaultRuntimeNodes());
    fixture->binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        fixture->runtime.scheduler().findNode(fixture->model.binder));
    fixture->sequencer =
        dynamic_cast<MediaActivatedStartupReleaseSequencerNode*>(
            fixture->runtime.scheduler().findNode(fixture->model.sequencer));
    fixture->group = fixture->runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("task4-group"));
    fixture->releaseInput = fixture->runtime.context().findInputChannel(
        fixture->model.binder, "release");
    fixture->transaction = fixture->runtime.context().findInputChannel(
        fixture->model.sequencer, "transaction");
    fixture->firstEvent = fixture->runtime.context().findInputChannel(
        fixture->model.firstActivatedSink, "in");
    fixture->secondEvent = fixture->runtime.context().findInputChannel(
        fixture->model.secondActivatedSink, "in");
    fixture->boundRelease = fixture->runtime.context().findInputChannel(
        fixture->model.releaseSink, "in");
    EXPECT_TRUE(ctx, fixture->binder && fixture->sequencer && fixture->group &&
                         fixture->releaseInput && fixture->transaction &&
                         fixture->firstEvent && fixture->secondEvent &&
                         fixture->boundRelease);
    return fixture;
}

void testBinderWaitsForTransactionCapacityBeforeActivation(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->group || !fixture->releaseInput ||
        !fixture->transaction) return;
    EXPECT_TRUE(ctx, fixture->transaction->push(
        makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0))));
    EXPECT_TRUE(ctx, fixture->releaseInput->push(release(ctx)));
    auto blocked = fixture->binder->process(fixture->runtime.context());
    EXPECT_TRUE(ctx, blocked &&
                         blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, fixture->boundRelease->size(), static_cast<std::size_t>(0));
}

void testSequencerCommitsOneEventToAllTargetsBeforeRelease(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer || !fixture->group ||
        !fixture->releaseInput) return;
    auto expectedRelease = release(ctx);
    EXPECT_TRUE(ctx, fixture->releaseInput->push(expectedRelease));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Active);
    MediaBufferRef first;
    MediaBufferRef second;
    MediaBufferRef forwarded;
    EXPECT_TRUE(ctx, fixture->firstEvent->tryPop(first));
    EXPECT_TRUE(ctx, fixture->secondEvent->tryPop(second));
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(forwarded));
    EXPECT_TRUE(ctx, first == second);
    EXPECT_TRUE(ctx, forwarded == expectedRelease);
    EXPECT_TRUE(ctx, dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
                         first.get()) != nullptr);
    EXPECT_EQ(ctx, fixture->firstEvent->metrics().pushed,
              static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, fixture->secondEvent->metrics().pushed,
              static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, fixture->boundRelease->metrics().pushed,
              static_cast<std::uint64_t>(1));
}

void testEveryBlockedSequencerTargetPreventsPrefixVisibility(TestContext& ctx)
{
    for (int blockedTarget = 0; blockedTarget != 3; ++blockedTarget) {
        auto fixture = startFixture(ctx);
        if (!fixture->binder || !fixture->sequencer || !fixture->releaseInput)
            continue;
        MediaChannel* blocked = blockedTarget == 0
            ? fixture->firstEvent
            : blockedTarget == 1 ? fixture->secondEvent : fixture->boundRelease;
        EXPECT_TRUE(ctx, blocked->push(
            makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0))));
        EXPECT_TRUE(ctx, fixture->releaseInput->push(release(ctx)));
        EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
        auto waiting = fixture->sequencer->process(fixture->runtime.context());
        EXPECT_TRUE(ctx, waiting &&
                             waiting.value().state == MediaNodeProcessState::Waiting);
        EXPECT_EQ(ctx, fixture->group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
        EXPECT_EQ(ctx, fixture->firstEvent->metrics().pushed,
                  blockedTarget == 0 ? static_cast<std::uint64_t>(1)
                                     : static_cast<std::uint64_t>(0));
        EXPECT_EQ(ctx, fixture->secondEvent->metrics().pushed,
                  blockedTarget == 1 ? static_cast<std::uint64_t>(1)
                                     : static_cast<std::uint64_t>(0));
        EXPECT_EQ(ctx, fixture->boundRelease->metrics().pushed,
                  blockedTarget == 2 ? static_cast<std::uint64_t>(1)
                                     : static_cast<std::uint64_t>(0));
        MediaBufferRef filler;
        EXPECT_TRUE(ctx, blocked->tryPop(filler));
        EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
        EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, fixture->boundRelease->size(), static_cast<std::size_t>(1));
    }
}

void testActivePassThroughDoesNotReactivate(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer || !fixture->releaseInput) return;
    EXPECT_TRUE(ctx, fixture->releaseInput->push(release(ctx)));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
    MediaBufferRef ignored;
    EXPECT_TRUE(ctx, fixture->firstEvent->tryPop(ignored));
    EXPECT_TRUE(ctx, fixture->secondEvent->tryPop(ignored));
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(ignored));

    const auto payload = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(2));
    auto passThrough = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("task4-group"),
        MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch(), origin(), {{payload, 0}}, {});
    EXPECT_TRUE(ctx, passThrough);
    EXPECT_TRUE(ctx, fixture->releaseInput->push(passThrough.value()));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
    MediaBufferRef first;
    MediaBufferRef second;
    MediaBufferRef forwarded;
    EXPECT_TRUE(ctx, fixture->firstEvent->tryPop(first));
    EXPECT_TRUE(ctx, fixture->secondEvent->tryPop(second));
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(forwarded));
    EXPECT_TRUE(ctx, first == second);
    EXPECT_TRUE(ctx, forwarded == passThrough.value());
}

void testClosedSequencerTargetFailsBeforeActivation(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer || !fixture->releaseInput)
        return;
    fixture->secondEvent->close();
    EXPECT_TRUE(ctx, fixture->releaseInput->push(release(ctx)));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    auto failed = fixture->sequencer->process(fixture->runtime.context());
    EXPECT_FALSE(ctx, failed);
    EXPECT_FALSE(ctx, fixture->sequencer->process(fixture->runtime.context()));
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, fixture->boundRelease->size(), static_cast<std::size_t>(0));
}

void testGenerationMismatchAndAbortedTargetFailClosed(TestContext& ctx)
{
    auto mismatch = startFixture(ctx);
    if (mismatch->binder && mismatch->sequencer && mismatch->releaseInput) {
        EXPECT_TRUE(ctx, mismatch->releaseInput->push(release(ctx)));
        EXPECT_TRUE(ctx, mismatch->binder->process(mismatch->runtime.context()));
        EXPECT_TRUE(ctx, mismatch->sequencer->process(mismatch->runtime.context()));
        MediaBufferRef ignored;
        EXPECT_TRUE(ctx, mismatch->firstEvent->tryPop(ignored));
        EXPECT_TRUE(ctx, mismatch->secondEvent->tryPop(ignored));
        EXPECT_TRUE(ctx, mismatch->boundRelease->tryPop(ignored));
        const auto payload = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(2));
        auto wrongGeneration = MediaAvStartupReleaseBuffer::create(
            MediaAvSyncGroupKey("task4-group"),
            MediaAvStartupReleaseKind::ActiveEpochPassThrough,
            epoch(2), origin(2), {{payload, 0}}, {});
        EXPECT_TRUE(ctx, wrongGeneration);
        EXPECT_TRUE(ctx, mismatch->releaseInput->push(wrongGeneration.value()));
        EXPECT_TRUE(ctx, mismatch->binder->process(mismatch->runtime.context()));
        EXPECT_FALSE(ctx, mismatch->sequencer->process(
                              mismatch->runtime.context()));
        EXPECT_EQ(ctx, mismatch->firstEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, mismatch->secondEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, mismatch->boundRelease->size(), static_cast<std::size_t>(0));
    }

    auto aborted = startFixture(ctx);
    if (aborted->binder && aborted->sequencer && aborted->releaseInput) {
        aborted->firstEvent->abort();
        EXPECT_TRUE(ctx, aborted->releaseInput->push(release(ctx)));
        EXPECT_TRUE(ctx, aborted->binder->process(aborted->runtime.context()));
        EXPECT_FALSE(ctx, aborted->sequencer->process(aborted->runtime.context()));
        EXPECT_EQ(ctx, aborted->group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
        EXPECT_EQ(ctx, aborted->secondEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, aborted->boundRelease->size(), static_cast<std::size_t>(0));
    }
}

void testGraphStopAndAbortClearQueuedTransactions(TestContext& ctx)
{
    const auto run = [&](bool abort) {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        graph.addOutputPort(source, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event,
                            MediaPayloadKind::GraphEvent);
        graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event,
                           MediaPayloadKind::GraphEvent);
        graph.connect(source, "out", sink, "in", "queued",
                      MediaGraphBuildSupport::blockingQueuePolicy(2));
        MediaGraphRuntime runtime;
        EXPECT_TRUE(ctx, runtime.compile(std::move(graph)));
        EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
            std::make_unique<WaitingNode>(source)));
        EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
            std::make_unique<WaitingNode>(sink)));
        MediaChannel* channel = runtime.context().findInputChannel(sink, "in");
        EXPECT_TRUE(ctx, channel != nullptr);
        if (!channel) return;
        EXPECT_TRUE(ctx, channel->push(
            makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0))));
        EXPECT_TRUE(ctx, runtime.startThreaded());
        if (abort) {
            runtime.abort();
        } else {
            EXPECT_TRUE(ctx, runtime.stop());
        }
        EXPECT_EQ(ctx, channel->size(), static_cast<std::size_t>(0));
    };
    run(false);
    run(true);
}

void testAbortDiscardsPendingReleaseTransactionAndFreshRuntimeStartsClean(
    TestContext& ctx)
{
    auto pending = startFixture(ctx);
    if (!pending->binder || !pending->sequencer || !pending->releaseInput)
        return;
    EXPECT_TRUE(ctx, pending->firstEvent->push(
        makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0))));
    EXPECT_TRUE(ctx, pending->releaseInput->push(release(ctx)));
    EXPECT_TRUE(ctx, pending->binder->process(pending->runtime.context()));
    auto waiting = pending->sequencer->process(pending->runtime.context());
    EXPECT_TRUE(ctx, waiting &&
                         waiting.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, pending->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    pending->runtime.abort();
    EXPECT_EQ(ctx, pending->releaseInput->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, pending->transaction->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, pending->firstEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, pending->secondEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, pending->boundRelease->size(), static_cast<std::size_t>(0));

    auto fresh = startFixture(ctx);
    if (!fresh->binder || !fresh->sequencer || !fresh->releaseInput) return;
    EXPECT_EQ(ctx, fresh->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    EXPECT_TRUE(ctx, fresh->releaseInput->push(release(ctx)));
    EXPECT_TRUE(ctx, fresh->binder->process(fresh->runtime.context()));
    EXPECT_TRUE(ctx, fresh->sequencer->process(fresh->runtime.context()));
    EXPECT_EQ(ctx, fresh->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Active);
}

void testTaskFourFactorySurfaceIsComplete(TestContext& ctx)
{
    for (const auto kind : {
             MediaNodeKind::ActivatedStartupReleaseSequencer,
             MediaNodeKind::RtpSourceClockStateAdapter,
             MediaNodeKind::AvStartupClock}) {
        EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(kind));
        MediaNode model{MediaNodeId{900 + static_cast<std::uint64_t>(kind)},
                        kind, "task4.factory"};
        const auto created = MediaRuntimeNodeFactory::create(model);
        EXPECT_EQ(ctx, static_cast<bool>(created),
                  kind != MediaNodeKind::ActivatedStartupReleaseSequencer);
    }
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
    testActivationReleaseTransactionPreservesReferences(ctx);
    testBinderWaitsForTransactionCapacityBeforeActivation(ctx);
    testSequencerCommitsOneEventToAllTargetsBeforeRelease(ctx);
    testEveryBlockedSequencerTargetPreventsPrefixVisibility(ctx);
    testActivePassThroughDoesNotReactivate(ctx);
    testClosedSequencerTargetFailsBeforeActivation(ctx);
    testGenerationMismatchAndAbortedTargetFailClosed(ctx);
    testGraphStopAndAbortClearQueuedTransactions(ctx);
    testAbortDiscardsPendingReleaseTransactionAndFreshRuntimeStartsClean(ctx);
    testTaskFourFactorySurfaceIsComplete(ctx);
    testRtpAdapterPublishesGenericLockedState(ctx);
    testStartupClockUsesRegisteredMasterDeadline(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
