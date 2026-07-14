#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaVideoRepeatRequestBuffer.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"

#include <atomic>
#include <array>
#include <chrono>
#include <concepts>
#include <limits>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

static_assert(std::is_move_constructible_v<MediaAvSyncGroupRegistry>);
static_assert(std::is_move_assignable_v<MediaAvSyncGroupRegistry>);
static_assert(std::is_move_constructible_v<MediaGraphExecutionContext>);
static_assert(std::is_move_assignable_v<MediaGraphExecutionContext>);
static_assert(!std::default_initializable<MediaScheduledAccessUnitParameters>);
static_assert(std::constructible_from<
    MediaScheduledAccessUnitParameters,
    MediaBufferRef,
    MediaScheduledStream,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    std::uint64_t,
    MediaSourceAccessUnitSequence,
    std::optional<MediaSourceAccessUnitSequence>,
    std::optional<MediaVideoRepeatRequestId>,
    std::optional<MediaVideoSyncDecisionKind>>);
static_assert(!std::constructible_from<
    MediaScheduledAccessUnitParameters,
    MediaBufferRef,
    MediaScheduledStream,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    MediaRunningTime,
    std::uint64_t,
    MediaSourceAccessUnitSequence,
    std::optional<MediaSourceAccessUnitSequence>,
    std::optional<MediaVideoRepeatRequestId>>);

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(MediaRunningTime value) : m_now(value.nanoseconds()) {}
    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(m_now.load()));
    }
    void set(MediaRunningTime value) noexcept { m_now.store(value.nanoseconds()); }
private:
    std::atomic<std::int64_t> m_now;
};

MediaAvSyncPlan completePlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "av-scheduler-matrix";
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
    auto plan = MediaAvSyncPlanner::plan(request);
    return std::move(plan).value();
}

template <typename Predicate>
bool waitUntil(Predicate predicate,
               std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

struct SchedulerFixture final {
    MediaGraph graph;
    MediaNodeId video;
    MediaNodeId audio;
    MediaNodeId scheduler;
    MediaNodeId sink;
};

SchedulerFixture graphWithScheduler(bool option = true,
                                    MediaQueueOverflowPolicy overflow =
                                        MediaQueueOverflowPolicy::BlockProducer,
                                    std::size_t outputCapacity = 8)
{
    SchedulerFixture f;
    f.video = f.graph.addNode(MediaNodeKind::Demux, "video");
    f.audio = f.graph.addNode(MediaNodeKind::Demux, "audio");
    f.scheduler = f.graph.addNode(MediaNodeKind::AvOutputScheduler, "scheduler");
    f.sink = f.graph.addNode(MediaNodeKind::RtpOutput, "sink");
    if (option) f.graph.setNodeOption(
        f.scheduler, "av_scheduler.sync_group", "matrix-group");
    f.graph.addOutputPort(f.video, "packet", MediaStreamKind::Video,
                          MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    f.graph.addOutputPort(f.audio, "packet", MediaStreamKind::Audio,
                          MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    f.graph.addInputPort(f.scheduler, "video", MediaStreamKind::Video,
                         MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    f.graph.addInputPort(f.scheduler, "audio", MediaStreamKind::Audio,
                         MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    f.graph.addOutputPort(f.scheduler, "scheduled", MediaStreamKind::Any,
                          MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    f.graph.addInputPort(f.sink, "scheduled", MediaStreamKind::Any,
                         MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    auto inputPolicy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    auto outputPolicy = MediaGraphBuildSupport::blockingQueuePolicy(outputCapacity);
    outputPolicy.queuePolicy.overflowPolicy = overflow;
    f.graph.connect(f.video, "packet", f.scheduler, "video", "video", inputPolicy);
    f.graph.connect(f.audio, "packet", f.scheduler, "audio", "audio", inputPolicy);
    f.graph.connect(f.scheduler, "scheduled", f.sink, "scheduled", "scheduled", outputPolicy);
    return f;
}

MediaBufferRef unit(TestContext& ctx, MediaScheduledStream stream,
                    std::int64_t pts, std::optional<std::int64_t> dts,
                    std::uint64_t generation, std::uint64_t sequence,
                    bool keyFrame = false,
                    MediaDecodeOrderMode order =
                        MediaDecodeOrderMode::PresentationOrderNoReorder)
{
    auto packet = makePacketBuffer(keyFrame, static_cast<std::int64_t>(sequence));
    EXPECT_TRUE(ctx, packet);
    if (!packet) return {};
    packet.value()->setStreamKind(stream == MediaScheduledStream::Video
                                      ? MediaStreamKind::Video
                                      : MediaStreamKind::Audio);
    auto result = MediaCanonicalAccessUnitBuffer::create(
        packet.value(), stream, ms(pts), dts ? std::optional(ms(*dts)) : std::nullopt,
        ms(10), order, generation, MediaSourceAccessUnitSequence(sequence));
    EXPECT_TRUE(ctx, result);
    return result ? std::move(result).value() : MediaBufferRef{};
}

MediaBufferRef repeatRequest(TestContext& ctx, std::int64_t pts,
                             std::uint64_t generation,
                             std::uint64_t requestId)
{
    auto result = MediaVideoRepeatRequestBuffer::create(
        ms(pts), ms(10), generation, MediaVideoRepeatRequestId(requestId));
    EXPECT_TRUE(ctx, result);
    return result ? std::move(result).value() : MediaBufferRef{};
}

const FFmpegPacketBuffer* scheduledPacket(const MediaScheduledAccessUnit* unit)
{
    return unit
        ? dynamic_cast<const FFmpegPacketBuffer*>(unit->media().get())
        : nullptr;
}

bool startFixture(TestContext& ctx, SchedulerFixture& f,
                  MediaGraphExecutionContext& execution,
                  const std::shared_ptr<MediaMasterClock>& clock,
                  MediaPlaybackEpoch epoch)
{
    if (!execution.compile(f.graph)) return false;
    if (!execution.registerAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"), completePlan(), clock)) return false;
    if (!execution.activatePlaybackEpoch(
            MediaAvSyncGroupKey("matrix-group"), epoch)) return false;
    return true;
}

std::vector<MediaScheduledStream> drainStreams(MediaGraphExecutionContext& execution,
                                                MediaNodeId sink)
{
    std::vector<MediaScheduledStream> streams;
    MediaBufferRef value;
    auto* output = execution.findInputChannel(sink, "scheduled");
    while (output->tryPop(value)) {
        if (const auto* scheduled =
                dynamic_cast<const MediaScheduledAccessUnit*>(value.get())) {
            streams.push_back(scheduled->stream());
        }
    }
    return streams;
}

void testSteadyClockOriginAndOverflow(TestContext& ctx)
{
    MediaSteadyMasterClock clock(ms(25));
    auto first = clock.now();
    auto second = clock.now();
    EXPECT_TRUE(ctx, first && second);
    if (first && second) {
        EXPECT_TRUE(ctx, first.value() >= ms(25));
        EXPECT_TRUE(ctx, second.value() >= first.value());
    }
    MediaSteadyMasterClock overflow(MediaRunningTime::fromNanoseconds(
        std::numeric_limits<std::int64_t>::max()));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_FALSE(ctx, overflow.now());
}

void testWakeupReportsDeadlineNotificationAndInterruption(TestContext& ctx)
{
    MediaNodeWakeup wakeup;
    auto observed = wakeup.sequence();
    EXPECT_EQ(ctx, wakeup.wait(observed, std::chrono::milliseconds(1)),
              MediaNodeWakeup::WaitOutcome::Deadline);
    observed = wakeup.sequence();
    std::thread notifier([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        wakeup.notify();
    });
    EXPECT_EQ(ctx, wakeup.wait(observed, std::chrono::milliseconds(100)),
              MediaNodeWakeup::WaitOutcome::Notified);
    notifier.join();
    observed = wakeup.sequence();
    std::thread interrupter([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        wakeup.interrupt();
    });
    EXPECT_EQ(ctx, wakeup.wait(observed, std::chrono::milliseconds(100)),
              MediaNodeWakeup::WaitOutcome::Interrupted);
    interrupter.join();
    wakeup.reset();
}

void testEpochLifecycleAndObservableReacquisition(TestContext& ctx)
{
    auto clock = std::make_shared<TestMasterClock>(ms(0));
    auto created = MediaAvSyncGroupRuntime::create(
        MediaAvSyncGroupKey("epoch"), completePlan(), clock);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto group = created.value();
    EXPECT_FALSE(ctx, group->playbackEpoch());
    EXPECT_TRUE(ctx, group->activatePlaybackEpoch({ms(10), ms(100), 1}));
    EXPECT_FALSE(ctx, group->activatePlaybackEpoch({ms(10), ms(100), 1}));
    auto mapped = group->mapCanonicalToMaster(ms(30));
    EXPECT_TRUE(ctx, mapped && mapped.value() == ms(120));
    auto future = group->observeGeneration(3);
    EXPECT_TRUE(ctx, future && future.value() ==
                         MediaAvSyncGroupRuntime::GenerationDisposition::ReacquisitionRequired);
    auto request = group->reacquisitionRequest();
    EXPECT_TRUE(ctx, request && request->observedGeneration == 3 &&
                         request->reason == MediaAvReacquisitionReason::FutureGeneration);
    EXPECT_TRUE(ctx, group->requestReacquisition(
        {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    EXPECT_TRUE(ctx, group->reacquisitionRequest() == request);
    EXPECT_FALSE(ctx, group->requestReacquisition(
        {1, MediaAvReacquisitionReason::FutureGeneration}));
    EXPECT_FALSE(ctx, group->requestReacquisition(
        {3, MediaAvReacquisitionReason::Flush}));
    EXPECT_FALSE(ctx, group->activateNextPlaybackEpoch({ms(10), ms(100), 2}));
    EXPECT_TRUE(ctx, group->activateNextPlaybackEpoch({ms(40), ms(200), 3}));
    EXPECT_FALSE(ctx, group->reacquisitionRequest());
    group->markAborted();
    auto afterAbort = group->observeGeneration(4);
    EXPECT_FALSE(ctx, afterAbort);
    EXPECT_EQ(ctx, group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_FALSE(ctx, group->reacquisitionRequest());
    EXPECT_FALSE(ctx, group->requestReacquisition(
        {3, MediaAvReacquisitionReason::HardDiscontinuity}));
    EXPECT_FALSE(ctx, group->activatePlaybackEpoch({ms(0), ms(0), 4}));

    auto discontinuity = MediaAvSyncGroupRuntime::create(
        MediaAvSyncGroupKey("hard"), completePlan(), clock);
    EXPECT_TRUE(ctx, discontinuity);
    EXPECT_TRUE(ctx, discontinuity.value()->activatePlaybackEpoch(
        {ms(0), ms(0), 5}));
    EXPECT_TRUE(ctx, discontinuity.value()->requestReacquisition(
        {5, MediaAvReacquisitionReason::HardDiscontinuity}));
    EXPECT_FALSE(ctx, discontinuity.value()->activateNextPlaybackEpoch(
        {ms(0), ms(0), 5}));
    EXPECT_TRUE(ctx, discontinuity.value()->activateNextPlaybackEpoch(
        {ms(50), ms(200), 6}));
    EXPECT_FALSE(ctx, discontinuity.value()->reacquisitionRequest());
}

void testDispatchOrderingPrecedesPresentationDecision(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(0));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 100, 0, 1, 1, true,
             MediaDecodeOrderMode::ReorderedRequiresDecodeTime)));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "audio")->push(
        unit(ctx, MediaScheduledStream::Audio, 50, 50, 1, 1)));
    execution.findInputChannel(f.scheduler, "video")->close();

    EXPECT_TRUE(ctx, scheduler.process(execution));
    auto video = drainStreams(execution, f.sink);
    EXPECT_EQ(ctx, video.size(), static_cast<std::size_t>(1));
    if (video.size() == 1) {
        EXPECT_EQ(ctx, video.front(), MediaScheduledStream::Video);
    }

    auto waiting = scheduler.process(execution);
    EXPECT_TRUE(ctx, waiting &&
        waiting.value().state == MediaNodeProcessState::Waiting &&
        waiting.value().deadlineWait.has_value());
    EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
    clock->set(ms(50));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    auto audio = drainStreams(execution, f.sink);
    EXPECT_EQ(ctx, audio.size(), static_cast<std::size_t>(1));
    if (audio.size() == 1) {
        EXPECT_EQ(ctx, audio.front(), MediaScheduledStream::Audio);
    }
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testQueuedAbortPreemptsMedia(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto* video = execution.findInputChannel(f.scheduler, "video");
    EXPECT_TRUE(ctx, video->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
    video->abort();

    EXPECT_FALSE(ctx, scheduler.process(execution));
    EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
    auto group = execution.findAvSyncGroup(MediaAvSyncGroupKey("matrix-group"));
    EXPECT_TRUE(ctx, group);
    if (group) {
        EXPECT_EQ(ctx, group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    }
    scheduler.abort(execution);
}

void testDualControlPriorityIsGlobalAndSymmetric(TestContext& ctx)
{
    const auto verifyWinner = [&](MediaControlBufferKind videoKind,
                                  MediaControlBufferKind audioKind,
                                  MediaControlBufferKind expectedKind,
                                  MediaAvSyncGroupRuntime::LifecycleState
                                      expectedState) {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            makeMediaBufferRef<MediaControlBuffer>(videoKind)));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "audio")->push(
            makeMediaBufferRef<MediaControlBuffer>(audioKind)));

        auto result = scheduler.process(execution);
        EXPECT_TRUE(ctx, result &&
            result.value().state == MediaNodeProcessState::Finished);
        auto group = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"));
        EXPECT_TRUE(ctx, group && group->lifecycleState() ==
            expectedState);
        if (expectedKind == MediaControlBufferKind::Flush) {
            auto request = group ? group->reacquisitionRequest() : std::nullopt;
            EXPECT_TRUE(ctx, request &&
                request->reason == MediaAvReacquisitionReason::Flush);
        } else {
            EXPECT_FALSE(ctx, group && group->reacquisitionRequest());
        }
        auto* output = execution.findInputChannel(f.sink, "scheduled");
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
        MediaBufferRef emitted;
        EXPECT_TRUE(ctx, output->tryPop(emitted));
        const auto* control = dynamic_cast<const MediaControlBuffer*>(
            emitted.get());
        EXPECT_TRUE(ctx, control &&
            control->controlKind() == expectedKind);
        auto afterTerminal = scheduler.process(execution);
        EXPECT_TRUE(ctx, afterTerminal &&
            afterTerminal.value().state == MediaNodeProcessState::Finished);
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
        scheduler.abort(execution);
    };

    const auto aborted = MediaAvSyncGroupRuntime::LifecycleState::Aborted;
    const auto reacquiring =
        MediaAvSyncGroupRuntime::LifecycleState::ReacquisitionRequired;
    verifyWinner(MediaControlBufferKind::Flush,
                 MediaControlBufferKind::Abort,
                 MediaControlBufferKind::Abort, aborted);
    verifyWinner(MediaControlBufferKind::Abort,
                 MediaControlBufferKind::Flush,
                 MediaControlBufferKind::Abort, aborted);
    verifyWinner(MediaControlBufferKind::Eof,
                 MediaControlBufferKind::Abort,
                 MediaControlBufferKind::Abort, aborted);
    verifyWinner(MediaControlBufferKind::Abort,
                 MediaControlBufferKind::Eof,
                 MediaControlBufferKind::Abort, aborted);
    verifyWinner(MediaControlBufferKind::Eof,
                 MediaControlBufferKind::Flush,
                 MediaControlBufferKind::Flush, reacquiring);
    verifyWinner(MediaControlBufferKind::Flush,
                 MediaControlBufferKind::Eof,
                 MediaControlBufferKind::Flush, reacquiring);
}

void testUnknownControlFailsClosedBeforeAbortSymmetrically(TestContext& ctx)
{
    const auto verifyUnknownWins = [&](bool unknownOnVideo) {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        auto unknown = makeMediaBufferRef<MediaControlBuffer>(
            MediaControlBufferKind::Unknown);
        unknown->setStreamKind(unknownOnVideo
            ? MediaStreamKind::Video : MediaStreamKind::Audio);
        unknown->setPayloadKind(MediaPayloadKind::Packet);
        auto abort = makeMediaBufferRef<MediaControlBuffer>(
            MediaControlBufferKind::Abort);
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            unknownOnVideo ? unknown : abort));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "audio")->push(
            unknownOnVideo ? abort : unknown));

        EXPECT_FALSE(ctx, scheduler.process(execution));
        auto group = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"));
        EXPECT_TRUE(ctx, group && group->lifecycleState() ==
            MediaAvSyncGroupRuntime::LifecycleState::Aborted);
        EXPECT_FALSE(ctx, group && group->reacquisitionRequest());
        auto* output = execution.findInputChannel(f.sink, "scheduled");
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
        EXPECT_FALSE(ctx, scheduler.process(execution));
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
        scheduler.abort(execution);
    };

    verifyUnknownWins(true);
    verifyUnknownWins(false);
}

void testSparseFutureGenerationRequestsReacquisitionImmediately(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 2, 1, true)));

    EXPECT_FALSE(ctx, scheduler.process(execution));
    auto request = execution.findAvSyncGroup(
        MediaAvSyncGroupKey("matrix-group"))->reacquisitionRequest();
    EXPECT_TRUE(ctx, request &&
        request->reason == MediaAvReacquisitionReason::FutureGeneration &&
        request->observedGeneration == 2);
    scheduler.abort(execution);
}

void testUnknownControlIsRejectedWithoutFlush(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto unknown = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Unknown);
    unknown->setStreamKind(MediaStreamKind::Video);
    unknown->setPayloadKind(MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unknown));

    EXPECT_FALSE(ctx, scheduler.process(execution));
    auto group = execution.findAvSyncGroup(MediaAvSyncGroupKey("matrix-group"));
    EXPECT_TRUE(ctx, group);
    if (group) {
        EXPECT_EQ(ctx, group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::Active);
        EXPECT_FALSE(ctx, group->reacquisitionRequest());
    }
    EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
    scheduler.abort(execution);
}

void testRegistryMoveKeepsSourceAndTargetUsable(TestContext& ctx)
{
    auto clock = std::make_shared<TestMasterClock>(ms(0));
    MediaAvSyncGroupRegistry source;
    EXPECT_TRUE(ctx, source.registerGroup(
        MediaAvSyncGroupKey("before-move"), completePlan(), clock));
    MediaAvSyncGroupRegistry target(std::move(source));
    EXPECT_TRUE(ctx, target.find(MediaAvSyncGroupKey("before-move")) != nullptr);
    EXPECT_TRUE(ctx, source.find(MediaAvSyncGroupKey("before-move")) == nullptr);
    EXPECT_TRUE(ctx, source.registerGroup(
        MediaAvSyncGroupKey("before-move"), completePlan(), clock));
    EXPECT_TRUE(ctx, source.find(MediaAvSyncGroupKey("before-move")) != nullptr);
    EXPECT_TRUE(ctx, target.find(MediaAvSyncGroupKey("before-move")) != nullptr);
    target.clear();
    source.clear();

    MediaAvSyncGroupRegistry assignedSource;
    MediaAvSyncGroupRegistry assignedTarget;
    EXPECT_TRUE(ctx, assignedSource.registerGroup(
        MediaAvSyncGroupKey("assigned-source"), completePlan(), clock));
    EXPECT_TRUE(ctx, assignedTarget.registerGroup(
        MediaAvSyncGroupKey("replaced-target"), completePlan(), clock));
    assignedTarget = std::move(assignedSource);
    EXPECT_TRUE(ctx, assignedTarget.find(
        MediaAvSyncGroupKey("assigned-source")) != nullptr);
    EXPECT_TRUE(ctx, assignedTarget.find(
        MediaAvSyncGroupKey("replaced-target")) == nullptr);
    EXPECT_TRUE(ctx, assignedSource.find(
        MediaAvSyncGroupKey("assigned-source")) == nullptr);
    EXPECT_TRUE(ctx, assignedSource.registerGroup(
        MediaAvSyncGroupKey("assigned-source"), completePlan(), clock));
}

void testDtsOrderingMappingAndEqualTimeRoundRobin(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(500));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(10), ms(100), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto* video = execution.findInputChannel(f.scheduler, "video");
    auto* audio = execution.findInputChannel(f.scheduler, "audio");
    EXPECT_TRUE(ctx, video->push(unit(ctx, MediaScheduledStream::Video, 200, 50, 1, 1,
                                      true, MediaDecodeOrderMode::ReorderedRequiresDecodeTime)));
    EXPECT_TRUE(ctx, audio->push(unit(ctx, MediaScheduledStream::Audio, 100, 100, 1, 1)));
    video->close();
    audio->close();
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    auto streams = drainStreams(execution, f.sink);
    EXPECT_EQ(ctx, streams.size(), static_cast<std::size_t>(2));
    if (streams.size() == 2) {
        EXPECT_EQ(ctx, streams[0], MediaScheduledStream::Video);
        EXPECT_EQ(ctx, streams[1], MediaScheduledStream::Audio);
    }
    EXPECT_TRUE(ctx, scheduler.stop(execution));

    auto media = makePacketBuffer(true, 9);
    EXPECT_TRUE(ctx, media);
    media.value()->setStreamKind(MediaStreamKind::Video);
    auto invalid = MediaCanonicalAccessUnitBuffer::create(
        media.value(), MediaScheduledStream::Video, ms(10), std::nullopt, ms(10),
        MediaDecodeOrderMode::ReorderedRequiresDecodeTime, 1,
        MediaSourceAccessUnitSequence(9));
    EXPECT_FALSE(ctx, invalid);
}

void testEqualTimeRoundRobinAcrossConsecutivePairs(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    for (std::uint64_t sequence = 1; sequence <= 2; ++sequence) {
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            unit(ctx, MediaScheduledStream::Video, 100, 100, 1, sequence, true)));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "audio")->push(
            unit(ctx, MediaScheduledStream::Audio, 100, 100, 1, sequence)));
    }
    execution.findInputChannel(f.scheduler, "video")->close();
    execution.findInputChannel(f.scheduler, "audio")->close();
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(ctx, scheduler.process(execution));
    const auto streams = drainStreams(execution, f.sink);
    EXPECT_EQ(ctx, streams.size(), static_cast<std::size_t>(4));
    if (streams.size() == 4) {
        EXPECT_EQ(ctx, streams[0], MediaScheduledStream::Audio);
        EXPECT_EQ(ctx, streams[1], MediaScheduledStream::Video);
        EXPECT_EQ(ctx, streams[2], MediaScheduledStream::Audio);
        EXPECT_EQ(ctx, streams[3], MediaScheduledStream::Video);
    }
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testRealDeadlineExpiresWithoutNotificationAndStopAbortInterrupt(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<MediaSteadyMasterClock>(ms(0));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 40, 40, 1, 1, true)));
    execution.findInputChannel(f.scheduler, "audio")->close();
    MediaGraphWorker worker(scheduler, execution);
    EXPECT_TRUE(ctx, worker.start());
    EXPECT_TRUE(ctx, waitUntil([&] { return worker.metrics().waits.load() >= 1; }));
    execution.nodeWakeup(f.scheduler).notify();
    EXPECT_TRUE(ctx, waitUntil([&] { return worker.metrics().wakeups.load() >= 1; }));
    EXPECT_TRUE(ctx, waitUntil([&] {
        return execution.findInputChannel(f.sink, "scheduled")->size() == 1;
    }, std::chrono::milliseconds(500)));
    EXPECT_TRUE(ctx, worker.metrics().deadlines.load() >= 1);
    worker.requestStop();
    worker.join();
    EXPECT_TRUE(ctx, scheduler.stop(execution));

    auto interrupt = [&](bool abort) {
        auto local = graphWithScheduler();
        MediaGraphExecutionContext localExecution;
        auto frozen = std::make_shared<TestMasterClock>(ms(0));
        EXPECT_TRUE(ctx, startFixture(ctx, local, localExecution, frozen,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode localScheduler(local.scheduler);
        EXPECT_TRUE(ctx, localScheduler.start(localExecution));
        EXPECT_TRUE(ctx, localExecution.findInputChannel(local.scheduler, "video")->push(
            unit(ctx, MediaScheduledStream::Video, 100, 100, 1, 1, true)));
        localExecution.findInputChannel(local.scheduler, "audio")->close();
        MediaGraphWorker localWorker(localScheduler, localExecution);
        EXPECT_TRUE(ctx, localWorker.start());
        EXPECT_TRUE(ctx, waitUntil(
            [&] { return localWorker.metrics().waits.load() >= 1; }));
        const auto calls = localWorker.metrics().processCalls.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        EXPECT_EQ(ctx, localWorker.metrics().processCalls.load(), calls);
        const auto began = std::chrono::steady_clock::now();
        if (abort) localWorker.abort(); else localWorker.requestStop();
        localWorker.join();
        EXPECT_TRUE(ctx, std::chrono::steady_clock::now() - began <
                             std::chrono::milliseconds(250));
        if (abort) localScheduler.abort(localExecution);
        else EXPECT_TRUE(ctx, localScheduler.stop(localExecution));
    };
    interrupt(false);
    interrupt(true);
}

void testOldGenerationDropFlushAndAbortedInput(TestContext& ctx)
{
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(400));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 2}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
        execution.findInputChannel(f.scheduler, "audio")->close();
        EXPECT_TRUE(ctx, scheduler.process(execution));
        EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
        EXPECT_TRUE(ctx, scheduler.stop(execution));
    }
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(1'000));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
        execution.findInputChannel(f.scheduler, "audio")->close();
        EXPECT_FALSE(ctx, scheduler.process(execution));
        auto request = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"))->reacquisitionRequest();
        EXPECT_TRUE(ctx, request &&
            request->reason == MediaAvReacquisitionReason::HardDiscontinuity &&
            request->observedGeneration == 1);
        EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
        scheduler.abort(execution);
    }
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "audio")->push(
            unit(ctx, MediaScheduledStream::Audio, 10, 10, 1, 1)));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Flush)));
        auto flushed = scheduler.process(execution);
        EXPECT_TRUE(ctx, flushed);
        auto request = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"))->reacquisitionRequest();
        EXPECT_TRUE(ctx, request &&
            request->reason == MediaAvReacquisitionReason::Flush &&
            request->observedGeneration == 1);
        EXPECT_TRUE(ctx, drainStreams(execution, f.sink).empty());
        scheduler.abort(execution);
    }
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(0));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        execution.findInputChannel(f.scheduler, "video")->abort();
        EXPECT_FALSE(ctx, scheduler.process(execution));
        scheduler.abort(execution);
    }
}

void testSparseStreamWaitsThenSingleEofAllowsOtherToContinue(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
    auto waiting = scheduler.process(execution);
    EXPECT_TRUE(ctx, waiting && waiting.value().state == MediaNodeProcessState::Waiting);
    execution.findInputChannel(f.scheduler, "audio")->close();
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_EQ(ctx, drainStreams(execution, f.sink).size(), static_cast<std::size_t>(1));
    execution.findInputChannel(f.scheduler, "video")->close();
    auto finished = scheduler.process(execution);
    EXPECT_TRUE(ctx, finished &&
        finished.value().state == MediaNodeProcessState::Finished);
    auto again = scheduler.process(execution);
    EXPECT_TRUE(ctx, again && again.value().state == MediaNodeProcessState::Finished);
    auto* output = execution.findInputChannel(f.sink, "scheduled");
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    MediaBufferRef terminal;
    EXPECT_TRUE(ctx, output->tryPop(terminal));
    const auto* eof = dynamic_cast<const MediaControlBuffer*>(terminal.get());
    EXPECT_TRUE(ctx, eof && eof->controlKind() == MediaControlBufferKind::Eof);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testBackpressureCommitsExactlyOnce(TestContext& ctx)
{
    auto f = graphWithScheduler(true, MediaQueueOverflowPolicy::BlockProducer, 1);
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto filler = makePacketBuffer(false, 1);
    EXPECT_TRUE(ctx, filler);
    filler.value()->setStreamKind(MediaStreamKind::Video);
    EXPECT_TRUE(ctx, execution.findInputChannel(f.sink, "scheduled")->push(filler.value()));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 20, 20, 1, 2, true)));
    execution.findInputChannel(f.scheduler, "audio")->close();
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_EQ(ctx, execution.findInputChannel(f.scheduler, "video")->size(),
              static_cast<std::size_t>(1));
    auto blocked = scheduler.process(execution);
    EXPECT_TRUE(ctx, blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, execution.findInputChannel(f.scheduler, "video")->size(),
              static_cast<std::size_t>(1));
    MediaBufferRef popped;
    EXPECT_TRUE(ctx, execution.findInputChannel(f.sink, "scheduled")->tryPop(popped));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    MediaBufferRef first;
    EXPECT_TRUE(ctx, execution.findInputChannel(f.sink, "scheduled")->tryPop(first));
    const auto* firstUnit = dynamic_cast<const MediaScheduledAccessUnit*>(first.get());
    EXPECT_TRUE(ctx, firstUnit && firstUnit->sourceSequence().value() == 1);
    EXPECT_TRUE(ctx, scheduler.process(execution));
    MediaBufferRef second;
    EXPECT_TRUE(ctx, execution.findInputChannel(f.sink, "scheduled")->tryPop(second));
    const auto* secondUnit = dynamic_cast<const MediaScheduledAccessUnit*>(second.get());
    EXPECT_TRUE(ctx, secondUnit && secondUnit->sourceSequence().value() == 2);
    EXPECT_EQ(ctx, execution.findInputChannel(f.sink, "scheduled")->size(),
              static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testRealPacketRepeatIdentityAndOwnership(TestContext& ctx)
{
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
            repeatRequest(ctx, 110, 1, 1)));
        execution.findInputChannel(f.scheduler, "audio")->close();
        EXPECT_FALSE(ctx, scheduler.process(execution));
        EXPECT_EQ(ctx, execution.findInputChannel(f.sink, "scheduled")->size(),
                  static_cast<std::size_t>(0));
        scheduler.abort(execution);
    }

    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                  {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));

    auto packet = makePacketBuffer(true, 9'001);
    EXPECT_TRUE(ctx, packet);
    auto* sourcePacket = dynamic_cast<FFmpegPacketBuffer*>(packet.value().get());
    EXPECT_TRUE(ctx, sourcePacket && sourcePacket->packet());
    if (sourcePacket && sourcePacket->packet()) {
        sourcePacket->packet()->dts = 8'999;
    }
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        packet.value(), MediaScheduledStream::Video, ms(100), ms(100), ms(10),
        MediaDecodeOrderMode::ReorderedRequiresDecodeTime, 1,
        MediaSourceAccessUnitSequence(41));
    EXPECT_TRUE(ctx, canonical);
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        canonical.value()));
    execution.findInputChannel(f.scheduler, "audio")->close();
    EXPECT_TRUE(ctx, scheduler.process(execution));

    MediaBufferRef originalOutput;
    auto* output = execution.findInputChannel(f.sink, "scheduled");
    EXPECT_TRUE(ctx, output->tryPop(originalOutput));
    const auto* original = dynamic_cast<const MediaScheduledAccessUnit*>(
        originalOutput.get());
    const auto* originalPacket = scheduledPacket(original);
    EXPECT_TRUE(ctx, original && originalPacket && originalPacket->packet());
    EXPECT_FALSE(ctx, original && original->repeatedFromSourceSequence());
    EXPECT_FALSE(ctx, original && original->repeatRequestId());
    const AVPacket* originalPacketAddress = originalPacket
        ? originalPacket->packet() : nullptr;
    const auto originalPts = originalPacketAddress
        ? originalPacketAddress->pts : AV_NOPTS_VALUE;
    const auto originalDts = originalPacketAddress
        ? originalPacketAddress->dts : AV_NOPTS_VALUE;

    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        repeatRequest(ctx, 110, 1, 77)));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    MediaBufferRef repeatedOutput;
    EXPECT_TRUE(ctx, output->tryPop(repeatedOutput));
    const auto* repeated = dynamic_cast<const MediaScheduledAccessUnit*>(
        repeatedOutput.get());
    const auto* repeatedPacket = scheduledPacket(repeated);
    EXPECT_TRUE(ctx, repeated && repeatedPacket && repeatedPacket->packet());
    EXPECT_TRUE(ctx, originalPacketAddress && repeatedPacket &&
                         originalPacketAddress != repeatedPacket->packet());
    EXPECT_EQ(ctx, originalPacketAddress->pts, originalPts);
    EXPECT_EQ(ctx, originalPacketAddress->dts, originalDts);
    EXPECT_EQ(ctx, repeatedPacket->packet()->pts, originalPts);
    EXPECT_EQ(ctx, repeatedPacket->packet()->dts, originalDts);
    EXPECT_EQ(ctx, repeated->presentationOnMaster(), ms(110));
    EXPECT_EQ(ctx, repeated->dispatchOnMaster(), ms(110));
    EXPECT_TRUE(ctx, repeated->repeatedFromSourceSequence() &&
                         repeated->repeatedFromSourceSequence()->value() == 41);
    EXPECT_TRUE(ctx, repeated->repeatRequestId() &&
                         repeated->repeatRequestId()->value() == 77);

    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        repeatRequest(ctx, 120, 1, 78)));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    MediaBufferRef secondRepeatOutput;
    EXPECT_TRUE(ctx, output->tryPop(secondRepeatOutput));
    const auto* secondRepeat = dynamic_cast<const MediaScheduledAccessUnit*>(
        secondRepeatOutput.get());
    EXPECT_TRUE(ctx, secondRepeat && secondRepeat->repeatRequestId() &&
                         secondRepeat->repeatRequestId()->value() == 78);
    EXPECT_TRUE(ctx, secondRepeat && secondRepeat->repeatedFromSourceSequence() &&
                         secondRepeat->repeatedFromSourceSequence()->value() == 41);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testScheduledAccessUnitRejectsNonOutputDecisions(TestContext& ctx)
{
    auto packet = makePacketBuffer(true, 1);
    EXPECT_TRUE(ctx, packet);
    const std::array rejected{
        MediaVideoSyncDecisionKind::Hold,
        MediaVideoSyncDecisionKind::Drop,
        MediaVideoSyncDecisionKind::Reacquire,
        MediaVideoSyncDecisionKind::NoAction,
        MediaVideoSyncDecisionKind::DropOldGeneration};
    for (const auto decision : rejected) {
        auto result = MediaScheduledAccessUnit::create(
            MediaScheduledAccessUnitParameters{
                packet.value(), MediaScheduledStream::Video,
                ms(1), ms(1), ms(1), ms(1), ms(10), 1,
                MediaSourceAccessUnitSequence(1), std::nullopt,
                std::nullopt, decision});
        EXPECT_FALSE(ctx, result);
    }

    auto mismatchedSource = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            packet.value(), MediaScheduledStream::Video,
            ms(2), ms(2), ms(2), ms(2), ms(10), 1,
            MediaSourceAccessUnitSequence(1),
            MediaSourceAccessUnitSequence(2),
            MediaVideoRepeatRequestId(1),
            MediaVideoSyncDecisionKind::RepeatPreviousFrame});
    EXPECT_FALSE(ctx, mismatchedSource);

    auto displayWithRepeatIdentity = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            packet.value(), MediaScheduledStream::Video,
            ms(2), ms(2), ms(2), ms(2), ms(10), 1,
            MediaSourceAccessUnitSequence(1),
            MediaSourceAccessUnitSequence(1),
            MediaVideoRepeatRequestId(1),
            MediaVideoSyncDecisionKind::Display});
    EXPECT_FALSE(ctx, displayWithRepeatIdentity);
}

void testAccessUnitFactoriesRejectNonPacketMedia(TestContext& ctx)
{
    auto audioFrame = FFmpegBufferFactory::wrapFrame(
        ::media::ffmpeg::makeFrame(), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, audioFrame);
    auto canonicalFrame = MediaCanonicalAccessUnitBuffer::create(
        audioFrame.value(), MediaScheduledStream::Audio,
        ms(1), ms(1), ms(10),
        MediaDecodeOrderMode::PresentationOrderNoReorder, 1,
        MediaSourceAccessUnitSequence(1));
    EXPECT_FALSE(ctx, canonicalFrame);

    auto videoFrame = FFmpegBufferFactory::wrapFrame(
        ::media::ffmpeg::makeFrame(), MediaStreamKind::Video);
    EXPECT_TRUE(ctx, videoFrame);
    auto scheduledFrame = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            videoFrame.value(), MediaScheduledStream::Video,
            ms(1), ms(1), ms(1), ms(1), ms(10), 1,
            MediaSourceAccessUnitSequence(1), std::nullopt,
            std::nullopt, MediaVideoSyncDecisionKind::Display});
    EXPECT_FALSE(ctx, scheduledFrame);

    auto nullPacket = makeMediaBufferRef<FFmpegPacketBuffer>(
        ::media::ffmpeg::PacketPtr{}, std::nullopt);
    nullPacket->setStreamKind(MediaStreamKind::Audio);
    nullPacket->setPayloadKind(MediaPayloadKind::Packet);
    auto canonicalNullPacket = MediaCanonicalAccessUnitBuffer::create(
        nullPacket, MediaScheduledStream::Audio,
        ms(1), ms(1), ms(10),
        MediaDecodeOrderMode::PresentationOrderNoReorder, 1,
        MediaSourceAccessUnitSequence(1));
    EXPECT_FALSE(ctx, canonicalNullPacket);

    nullPacket->setStreamKind(MediaStreamKind::Video);
    auto scheduledNullPacket = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            nullPacket, MediaScheduledStream::Video,
            ms(1), ms(1), ms(1), ms(1), ms(10), 1,
            MediaSourceAccessUnitSequence(1), std::nullopt,
            std::nullopt, MediaVideoSyncDecisionKind::Display});
    EXPECT_FALSE(ctx, scheduledNullPacket);
}

void testRepeatBackpressureCommitsIdentityExactlyOnce(TestContext& ctx)
{
    auto f = graphWithScheduler(true, MediaQueueOverflowPolicy::BlockProducer, 1);
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                  {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto* video = execution.findInputChannel(f.scheduler, "video");
    auto* output = execution.findInputChannel(f.sink, "scheduled");
    execution.findInputChannel(f.scheduler, "audio")->close();
    EXPECT_TRUE(ctx, video->push(
        unit(ctx, MediaScheduledStream::Video, 100, 100, 1, 51, true)));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    MediaBufferRef original;
    EXPECT_TRUE(ctx, output->tryPop(original));

    auto filler = makePacketBuffer(false, 1);
    EXPECT_TRUE(ctx, filler);
    filler.value()->setStreamKind(MediaStreamKind::Video);
    EXPECT_TRUE(ctx, output->push(filler.value()));
    EXPECT_TRUE(ctx, video->push(repeatRequest(ctx, 110, 1, 901)));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    auto waitingOne = scheduler.process(execution);
    auto waitingTwo = scheduler.process(execution);
    EXPECT_TRUE(ctx, waitingOne &&
        waitingOne.value().state == MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, waitingTwo &&
        waitingTwo.value().state == MediaNodeProcessState::Waiting);

    MediaBufferRef popped;
    EXPECT_TRUE(ctx, output->tryPop(popped));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_TRUE(ctx, scheduler.process(execution));
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    MediaBufferRef repeatedOutput;
    EXPECT_TRUE(ctx, output->tryPop(repeatedOutput));
    const auto* repeated = dynamic_cast<const MediaScheduledAccessUnit*>(
        repeatedOutput.get());
    EXPECT_TRUE(ctx, repeated && repeated->repeatRequestId() &&
                         repeated->repeatRequestId()->value() == 901);
    EXPECT_TRUE(ctx, repeated && repeated->repeatedFromSourceSequence() &&
                         repeated->repeatedFromSourceSequence()->value() == 51);
    auto afterCommit = scheduler.process(execution);
    EXPECT_TRUE(ctx, afterCommit &&
        afterCommit.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testFlushCancelsPendingBackpressureTransfer(TestContext& ctx)
{
    auto f = graphWithScheduler(true, MediaQueueOverflowPolicy::BlockProducer, 1);
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto filler = makePacketBuffer(false, 1);
    EXPECT_TRUE(ctx, filler);
    filler.value()->setStreamKind(MediaStreamKind::Video);
    auto* output = execution.findInputChannel(f.sink, "scheduled");
    EXPECT_TRUE(ctx, output->push(filler.value()));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
    execution.findInputChannel(f.scheduler, "audio")->close();
    auto blocked = scheduler.process(execution);
    EXPECT_TRUE(ctx, blocked &&
        blocked.value().state == MediaNodeProcessState::Waiting);

    EXPECT_TRUE(ctx, scheduler.flush(execution));
    auto group = execution.findAvSyncGroup(MediaAvSyncGroupKey("matrix-group"));
    auto request = group->reacquisitionRequest();
    EXPECT_TRUE(ctx, request &&
        request->reason == MediaAvReacquisitionReason::Flush &&
        request->observedGeneration == 1);
    MediaBufferRef popped;
    EXPECT_TRUE(ctx, output->tryPop(popped));
    EXPECT_FALSE(ctx, scheduler.process(execution));
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void verifyBackpressuredTerminalControl(
    TestContext& ctx,
    MediaControlBufferKind kind,
    MediaAvSyncGroupRuntime::LifecycleState expectedState)
{
    auto f = graphWithScheduler(true, MediaQueueOverflowPolicy::BlockProducer, 1);
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    auto filler = makePacketBuffer(false, 1);
    EXPECT_TRUE(ctx, filler);
    filler.value()->setStreamKind(MediaStreamKind::Video);
    auto* output = execution.findInputChannel(f.sink, "scheduled");
    EXPECT_TRUE(ctx, output->push(filler.value()));

    auto terminal = makeMediaBufferRef<MediaControlBuffer>(kind);
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        terminal));
    auto blocked = scheduler.process(execution);
    EXPECT_TRUE(ctx, blocked &&
        blocked.value().state == MediaNodeProcessState::Waiting);

    auto group = execution.findAvSyncGroup(MediaAvSyncGroupKey("matrix-group"));
    EXPECT_TRUE(ctx, group);
    if (group) EXPECT_EQ(ctx, group->lifecycleState(), expectedState);
    if (kind == MediaControlBufferKind::Flush) {
        auto request = group->reacquisitionRequest();
        EXPECT_TRUE(ctx, request &&
            request->reason == MediaAvReacquisitionReason::Flush &&
            request->observedGeneration == 1);
    } else {
        EXPECT_FALSE(ctx, group->reacquisitionRequest());
    }

    MediaBufferRef popped;
    EXPECT_TRUE(ctx, output->tryPop(popped));
    auto drained = scheduler.process(execution);
    EXPECT_TRUE(ctx, drained &&
        drained.value().state == MediaNodeProcessState::Progress);
    auto finished = scheduler.process(execution);
    EXPECT_TRUE(ctx, finished &&
        finished.value().state == MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    MediaBufferRef emitted;
    EXPECT_TRUE(ctx, output->tryPop(emitted));
    const auto* control = dynamic_cast<const MediaControlBuffer*>(emitted.get());
    EXPECT_TRUE(ctx, control && control->controlKind() == kind);
    auto again = scheduler.process(execution);
    EXPECT_TRUE(ctx, again &&
        again.value().state == MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, scheduler.stop(execution));
}

void testAbortPreflightDiscardsCachedAndPendingOutput(TestContext& ctx)
{
    {
        auto f = graphWithScheduler();
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        auto* video = execution.findInputChannel(f.scheduler, "video");
        EXPECT_TRUE(ctx, video->push(
            unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
        auto waiting = scheduler.process(execution);
        EXPECT_TRUE(ctx, waiting &&
            waiting.value().state == MediaNodeProcessState::Waiting);
        EXPECT_EQ(ctx, video->size(), static_cast<std::size_t>(0));

        video->abort();
        EXPECT_FALSE(ctx, scheduler.process(execution));
        auto group = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"));
        EXPECT_TRUE(ctx, group && group->lifecycleState() ==
            MediaAvSyncGroupRuntime::LifecycleState::Aborted);
        EXPECT_EQ(ctx,
            execution.findInputChannel(f.sink, "scheduled")->size(),
            static_cast<std::size_t>(0));
        scheduler.abort(execution);
    }

    {
        auto f = graphWithScheduler(
            true, MediaQueueOverflowPolicy::BlockProducer, 1);
        MediaGraphExecutionContext execution;
        auto clock = std::make_shared<TestMasterClock>(ms(100));
        EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock,
                                      {ms(0), ms(0), 1}));
        MediaAvOutputSchedulerNode scheduler(f.scheduler);
        EXPECT_TRUE(ctx, scheduler.start(execution));
        auto filler = makePacketBuffer(false, 1);
        EXPECT_TRUE(ctx, filler);
        filler.value()->setStreamKind(MediaStreamKind::Video);
        auto* output = execution.findInputChannel(f.sink, "scheduled");
        EXPECT_TRUE(ctx, output->push(filler.value()));
        auto* video = execution.findInputChannel(f.scheduler, "video");
        EXPECT_TRUE(ctx, video->push(
            unit(ctx, MediaScheduledStream::Video, 10, 10, 1, 1, true)));
        execution.findInputChannel(f.scheduler, "audio")->close();
        auto blocked = scheduler.process(execution);
        EXPECT_TRUE(ctx, blocked &&
            blocked.value().state == MediaNodeProcessState::Waiting);

        video->abort();
        EXPECT_FALSE(ctx, scheduler.process(execution));
        MediaBufferRef popped;
        EXPECT_TRUE(ctx, output->tryPop(popped));
        EXPECT_FALSE(ctx, scheduler.process(execution));
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
        auto group = execution.findAvSyncGroup(
            MediaAvSyncGroupKey("matrix-group"));
        EXPECT_TRUE(ctx, group && group->lifecycleState() ==
            MediaAvSyncGroupRuntime::LifecycleState::Aborted);
        scheduler.abort(execution);
    }
}

void testBackpressuredFlushAndAbortCommitExactlyOnce(TestContext& ctx)
{
    verifyBackpressuredTerminalControl(
        ctx, MediaControlBufferKind::Flush,
        MediaAvSyncGroupRuntime::LifecycleState::ReacquisitionRequired);
    verifyBackpressuredTerminalControl(
        ctx, MediaControlBufferKind::Abort,
        MediaAvSyncGroupRuntime::LifecycleState::Aborted);
}

void testFutureGenerationFlushAbortAndConfigurationFailures(TestContext& ctx)
{
    auto f = graphWithScheduler();
    MediaGraphExecutionContext execution;
    auto clock = std::make_shared<TestMasterClock>(ms(100));
    EXPECT_TRUE(ctx, startFixture(ctx, f, execution, clock, {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode scheduler(f.scheduler);
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(f.scheduler, "video")->push(
        unit(ctx, MediaScheduledStream::Video, 10, 10, 2, 1, true)));
    execution.findInputChannel(f.scheduler, "audio")->close();
    EXPECT_FALSE(ctx, scheduler.process(execution));
    auto group = execution.findAvSyncGroup(MediaAvSyncGroupKey("matrix-group"));
    auto request = group->reacquisitionRequest();
    EXPECT_TRUE(ctx, request && request->observedGeneration == 2 &&
                         request->reason == MediaAvReacquisitionReason::FutureGeneration);
    scheduler.abort(execution);
    auto missingOption = graphWithScheduler(false);
    MediaGraphExecutionContext missingOptionExecution;
    EXPECT_TRUE(ctx, missingOptionExecution.compile(missingOption.graph));
    MediaAvOutputSchedulerNode missingOptionNode(missingOption.scheduler);
    EXPECT_FALSE(ctx, missingOptionNode.start(missingOptionExecution));

    auto missingGroup = graphWithScheduler();
    MediaGraphExecutionContext missingGroupExecution;
    EXPECT_TRUE(ctx, missingGroupExecution.compile(missingGroup.graph));
    MediaAvOutputSchedulerNode missingGroupNode(missingGroup.scheduler);
    EXPECT_FALSE(ctx, missingGroupNode.start(missingGroupExecution));
    EXPECT_TRUE(ctx, missingGroupExecution.registerAvSyncGroup(
        MediaAvSyncGroupKey("matrix-group"), completePlan(), clock));
    EXPECT_TRUE(ctx, missingGroupExecution.activatePlaybackEpoch(
        MediaAvSyncGroupKey("matrix-group"), {ms(0), ms(0), 1}));
    EXPECT_TRUE(ctx, missingGroupNode.start(missingGroupExecution));
    EXPECT_TRUE(ctx, missingGroupNode.stop(missingGroupExecution));

    auto nonblocking = graphWithScheduler(
        true, MediaQueueOverflowPolicy::DropNewest, 8);
    MediaGraphExecutionContext nonblockingExecution;
    EXPECT_TRUE(ctx, startFixture(ctx, nonblocking, nonblockingExecution, clock,
                                  {ms(0), ms(0), 1}));
    MediaAvOutputSchedulerNode nonblockingNode(nonblocking.scheduler);
    EXPECT_FALSE(ctx, nonblockingNode.start(nonblockingExecution));
}

void testFactoryCreatesAvOutputScheduler(TestContext& ctx)
{
    MediaNode factoryNode{MediaNodeId{900}, MediaNodeKind::AvOutputScheduler,
                          "factory.scheduler"};
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(
                         MediaNodeKind::AvOutputScheduler));
    auto created = MediaRuntimeNodeFactory::create(factoryNode);
    EXPECT_TRUE(ctx, created);
    if (created) std::move(created).value().reset();

    MediaGraph graph;
    graph.addNode(MediaNodeKind::AvOutputScheduler, "dump.scheduler");
    EXPECT_TRUE(ctx, MediaGraphDump::toText(graph).find("AvOutputScheduler") !=
                         std::string::npos);
    EXPECT_EQ(ctx, std::string(mediaGraphDiagnosticNodeKindName(
                       MediaNodeKind::AvOutputScheduler)),
              std::string("AvOutputScheduler"));
}

} // namespace

void runAvOutputSchedulerTests(media_transcode::test::TestContext& ctx)
{
    testSteadyClockOriginAndOverflow(ctx);
    testWakeupReportsDeadlineNotificationAndInterruption(ctx);
    testEpochLifecycleAndObservableReacquisition(ctx);
    testRegistryMoveKeepsSourceAndTargetUsable(ctx);
    testDtsOrderingMappingAndEqualTimeRoundRobin(ctx);
    testDispatchOrderingPrecedesPresentationDecision(ctx);
    testEqualTimeRoundRobinAcrossConsecutivePairs(ctx);
    testRealDeadlineExpiresWithoutNotificationAndStopAbortInterrupt(ctx);
    testSparseStreamWaitsThenSingleEofAllowsOtherToContinue(ctx);
    testBackpressureCommitsExactlyOnce(ctx);
    testRealPacketRepeatIdentityAndOwnership(ctx);
    testScheduledAccessUnitRejectsNonOutputDecisions(ctx);
    testAccessUnitFactoriesRejectNonPacketMedia(ctx);
    testRepeatBackpressureCommitsIdentityExactlyOnce(ctx);
    testFlushCancelsPendingBackpressureTransfer(ctx);
    testBackpressuredFlushAndAbortCommitExactlyOnce(ctx);
    testAbortPreflightDiscardsCachedAndPendingOutput(ctx);
    testOldGenerationDropFlushAndAbortedInput(ctx);
    testQueuedAbortPreemptsMedia(ctx);
    testDualControlPriorityIsGlobalAndSymmetric(ctx);
    testUnknownControlFailsClosedBeforeAbortSymmetrically(ctx);
    testSparseFutureGenerationRequestsReacquisitionImmediately(ctx);
    testUnknownControlIsRejectedWithoutFlush(ctx);
    testFutureGenerationFlushAbortAndConfigurationFailures(ctx);
    testFactoryCreatesAvOutputScheduler(ctx);
}
