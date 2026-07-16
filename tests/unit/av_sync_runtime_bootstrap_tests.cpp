#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <chrono>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaAvSyncPlan completeRtpPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "runtime-bootstrap";
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

MediaGraph graphWithGroupReference(const std::optional<std::string>& group)
{
    MediaGraph graph;
    const auto node = graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    const auto binder = graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "epoch-binder");
    const auto sequencer = graph.addNode(
        MediaNodeKind::ActivatedStartupReleaseSequencer,
        "activation-release-sequencer");
    graph.addNode(MediaNodeKind::Finalize, "sink");
    if (group) {
        graph.setNodeOption(node, "av_scheduler.sync_group", *group);
        graph.setNodeOption(binder, "playback_epoch_binder.sync_group", *group);
        graph.setNodeOption(
            sequencer,
            "activated_startup_release_sequencer.sync_group", *group);
    }
    return graph;
}

MediaAvGenerationTransitionPlan completeTransitionPlan(
    MediaAvSyncOutputAdapterKind adapter =
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp)
{
    return MediaAvGenerationTransitionPlanner::plan(
        adapter, ms(1'000), ms(500));
}

MediaGraph graphWithInvalidGroupReference(MediaNodeKind kind,
                                          std::string key)
{
    MediaGraph graph;
    const auto node = graph.addNode(kind, "invalid.consumer");
    graph.setNodeOption(node, std::move(key), "realtime.av");
    return graph;
}

MediaRealtimeExecutableGraph executableWith(
    MediaAvSyncPlan plan,
    std::string bindingGroup = "realtime.av",
    std::optional<std::string> nodeGroup = std::string("realtime.av"))
{
    MediaRealtimeExecutableGraph executable;
    executable.graph = graphWithGroupReference(nodeGroup);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey(std::move(bindingGroup)), std::move(plan),
        completeTransitionPlan()});
    return executable;
}

class DeterministicClockSource final : public MediaAvSyncClockSource {
public:
    ::media::Result<MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) override
    {
        ++captures;
        auto clock = std::make_shared<MediaSteadyMasterClock>(ms(123));
        std::shared_ptr<const MediaSharedNtpEpoch> epoch;
        if (requireSharedNtpEpoch) {
            auto created = MediaSharedNtpEpoch::create(
                ms(123), std::chrono::seconds(1'700'000'000));
            if (!created) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    created.error());
            }
            epoch = std::make_shared<const MediaSharedNtpEpoch>(
                std::move(created).value());
        }
        lastClock = clock;
        lastEpoch = epoch;
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{std::move(clock), std::move(epoch)});
    }

    int captures = 0;
    std::weak_ptr<MediaSteadyMasterClock> lastClock;
    std::weak_ptr<const MediaSharedNtpEpoch> lastEpoch;
};

class InvalidClockSource final : public MediaAvSyncClockSource {
public:
    ::media::Result<MediaAvSyncClockBundle> capture(bool) override
    {
        auto clock = std::make_shared<MediaSteadyMasterClock>(ms(0));
        capturedClock = clock;
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{std::move(clock), nullptr});
    }

    std::weak_ptr<MediaSteadyMasterClock> capturedClock;
};

struct DeadlineNodeState final {
    std::atomic_size_t processCalls{0};
    std::atomic_bool interrupted{false};
    std::atomic_bool stopped{false};
    std::atomic_bool aborted{false};
};

class DeadlineRuntimeNode final : public MediaRuntimeNode {
public:
    DeadlineRuntimeNode(MediaNodeId id,
                        MediaAvSyncGroupKey group,
                        std::shared_ptr<DeadlineNodeState> state)
        : m_id(id), m_group(std::move(group)), m_state(std::move(state))
    {
    }

    MediaNodeId nodeId() const noexcept override { return m_id; }

    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext&) override
    {
        ++m_state->processCalls;
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(m_group, ms(3'600'000)));
    }

    ::media::Status stop(MediaGraphExecutionContext&) override
    {
        m_state->stopped = true;
        return ::media::Status::success();
    }

    void interrupt(MediaGraphExecutionContext&) noexcept override
    {
        m_state->interrupted = true;
    }

    void abort(MediaGraphExecutionContext&) noexcept override
    {
        m_state->aborted = true;
    }

private:
    MediaNodeId m_id;
    MediaAvSyncGroupKey m_group;
    std::shared_ptr<DeadlineNodeState> m_state;
};

bool waitForDeadlineWait(const MediaGraphRuntime& runtime,
                         const DeadlineNodeState& state)
{
    const auto timeout = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < timeout) {
        if (state.processCalls.load() != 0 &&
            runtime.threadedExecutor().metrics().workerWaits != 0) {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

void testBootstrapRejectsIncompleteBindings(TestContext& ctx)
{
    MediaRealtimeExecutableGraph missing;
    missing.graph = graphWithGroupReference(std::string("realtime.av"));
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(missing));

    auto invalid = executableWith(completeRtpPlan(), "");
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(invalid));

    auto mismatch = executableWith(
        completeRtpPlan(), "realtime.av", std::string("another.group"));
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(mismatch));

    auto orphan = executableWith(
        completeRtpPlan(), "realtime.av", std::nullopt);
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(orphan));

    auto matching = executableWith(completeRtpPlan());
    EXPECT_TRUE(ctx, MediaGraphRuntimeCompiler::validateBindings(matching));

    auto demuxSuffix = executableWith(completeRtpPlan());
    demuxSuffix.graph = graphWithInvalidGroupReference(
        MediaNodeKind::Demux, "runtime.sync_group");
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(demuxSuffix));

    auto legacyMux = executableWith(completeRtpPlan());
    legacyMux.graph = graphWithInvalidGroupReference(
        MediaNodeKind::RtpMux, "av_sync.sync_group");
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(legacyMux));

    auto wrongSchedulerKey = executableWith(completeRtpPlan());
    wrongSchedulerKey.graph = graphWithInvalidGroupReference(
        MediaNodeKind::AvOutputScheduler, "runtime.sync_group");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     wrongSchedulerKey));
}

void testBootstrapCapturesOneClockAndOneRtpEpoch(TestContext& ctx)
{
    const MediaAvSyncRuntimeBinding binding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan()};
    auto source = std::make_shared<DeterministicClockSource>();
    auto clocks = MediaAvSyncRuntimeBootstrap::createClocks(binding, *source);
    EXPECT_TRUE(ctx, clocks);
    EXPECT_EQ(ctx, source->captures, 1);
    if (!clocks) return;
    EXPECT_TRUE(ctx, clocks.value().masterClock != nullptr);
    EXPECT_TRUE(ctx, clocks.value().sharedNtpEpoch != nullptr);
    EXPECT_TRUE(ctx, clocks.value().masterClock == source->lastClock.lock());
    EXPECT_TRUE(ctx, clocks.value().sharedNtpEpoch == source->lastEpoch.lock());

    MediaGraphRuntime runtime(source);
    MediaRealtimeExecutableGraph executable;
    executable.graph = graphWithGroupReference(std::string("realtime.av"));
    executable.avSyncBinding.emplace(binding);
    EXPECT_TRUE(ctx, runtime.compile(std::move(executable)));
    EXPECT_EQ(ctx, source->captures, 2);
    auto group = runtime.context().findAvSyncGroup(binding.groupKey);
    EXPECT_TRUE(ctx, group != nullptr);
    if (!group) return;
    EXPECT_EQ(ctx, group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    EXPECT_TRUE(ctx, group->clock() == source->lastClock.lock());
    EXPECT_TRUE(ctx, group->sharedNtpEpoch() == source->lastEpoch.lock());
    const auto firstSharedEpoch = group->sharedNtpEpoch();
    const auto secondSharedEpoch = group->sharedNtpEpoch();
    EXPECT_TRUE(ctx, firstSharedEpoch.get() == secondSharedEpoch.get());
    EXPECT_FALSE(ctx, group->playbackEpoch());
    runtime.context().shutdownAvSyncGroups();
    runtime.context().shutdownAvSyncGroups();
    EXPECT_EQ(ctx, group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(binding.groupKey) == nullptr);

    InvalidClockSource invalidSource;
    EXPECT_FALSE(ctx, MediaAvSyncRuntimeBootstrap::createClocks(
                          binding, invalidSource));
    EXPECT_TRUE(ctx, invalidSource.capturedClock.expired());
}

void testTsClockCaptureDoesNotCreateNtpEpoch(TestContext& ctx)
{
    auto tsPlan = completeRtpPlan();
    tsPlan.topology = MediaAvSyncTopology::MpegTsToMpegTs;
    tsPlan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    tsPlan.rtp.reset();
    tsPlan.ts.emplace();
    tsPlan.ts->programNumber = 1;
    tsPlan.ts->programMapPid = 0x100;
    tsPlan.ts->videoPid = 0x101;
    tsPlan.ts->audioPid = 0x102;
    tsPlan.ts->pcrPid = 0x101;
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        ms(100), 0x1B, 0x0F, MediaTsH264InputLayout::LengthPrefixed,
        4, MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{ms(20), ms(100), ms(5), 1, 90'000},
        ms(500), 188, MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024});
    EXPECT_TRUE(ctx, mux);
    if (!mux) return;
    tsPlan.ts->outputMux = std::move(mux).value();

    const MediaAvSyncRuntimeBinding binding{
        MediaAvSyncGroupKey("realtime.av"), std::move(tsPlan),
        completeTransitionPlan(MediaAvSyncOutputAdapterKind::ProjectMpegTs)};
    DeterministicClockSource source;
    auto clocks = MediaAvSyncRuntimeBootstrap::createClocks(binding, source);
    EXPECT_TRUE(ctx, clocks);
    EXPECT_EQ(ctx, source.captures, 1);
    if (clocks) EXPECT_TRUE(ctx, clocks.value().sharedNtpEpoch == nullptr);
}

enum class ThreadedLifecycle { Stop, Abort, Reset };

void expectThreadedLifecycleCleanup(TestContext& ctx,
                                    ThreadedLifecycle lifecycle)
{
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(executableWith(completeRtpPlan())));
    auto group = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av"));
    EXPECT_TRUE(ctx, group != nullptr);
    auto nodeState = std::make_shared<DeadlineNodeState>();
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
                         std::make_unique<DeadlineRuntimeNode>(
                             runtime.graph()->nodes().front().id,
                             MediaAvSyncGroupKey("realtime.av"),
                             nodeState)));
    EXPECT_TRUE(ctx, runtime.startThreaded());
    EXPECT_TRUE(ctx, waitForDeadlineWait(runtime, *nodeState));
    switch (lifecycle) {
    case ThreadedLifecycle::Stop:
        EXPECT_TRUE(ctx, runtime.stop());
        break;
    case ThreadedLifecycle::Abort:
        runtime.abort();
        break;
    case ThreadedLifecycle::Reset:
        runtime.reset();
        break;
    }
    EXPECT_TRUE(ctx, nodeState->interrupted.load());
    EXPECT_EQ(ctx, nodeState->stopped.load(),
              lifecycle == ThreadedLifecycle::Stop);
    EXPECT_EQ(ctx, nodeState->aborted.load(),
              lifecycle != ThreadedLifecycle::Stop);
    EXPECT_TRUE(ctx, group && group->lifecycleState() ==
        MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av")) == nullptr);
}

void testRuntimeRollbackAndLifecycleCleanup(TestContext& ctx)
{
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(executableWith(completeRtpPlan())));
    auto original = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av"));
    const MediaGraph* originalGraph = runtime.graph();
    const auto originalState = runtime.state();
    auto mismatched = executableWith(
        completeRtpPlan(), "realtime.av", std::string("wrong.group"));
    EXPECT_FALSE(ctx, runtime.compile(std::move(mismatched)));
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av")) == original);
    EXPECT_TRUE(ctx, runtime.graph() == originalGraph);
    EXPECT_EQ(ctx, runtime.state(), originalState);
    EXPECT_TRUE(ctx, original && original->lifecycleState() ==
        MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);

    expectThreadedLifecycleCleanup(ctx, ThreadedLifecycle::Abort);
    expectThreadedLifecycleCleanup(ctx, ThreadedLifecycle::Reset);
    MediaGraphRuntime recompiled;
    EXPECT_TRUE(ctx, recompiled.compile(executableWith(completeRtpPlan())));
    auto recompiledOldGroup = recompiled.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av"));
    MediaGraph unsynchronized;
    unsynchronized.addNode(MediaNodeKind::Finalize, "unsynchronized");
    EXPECT_TRUE(ctx, recompiled.compile(std::move(unsynchronized)));
    EXPECT_TRUE(ctx, recompiledOldGroup &&
        recompiledOldGroup->lifecycleState() ==
            MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_TRUE(ctx, recompiled.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av")) == nullptr);

    expectThreadedLifecycleCleanup(ctx, ThreadedLifecycle::Stop);
}

} // namespace

void runAvSyncRuntimeBootstrapTests(
    media_transcode::test::TestContext& ctx)
{
    testBootstrapRejectsIncompleteBindings(ctx);
    testBootstrapCapturesOneClockAndOneRtpEpoch(ctx);
    testTsClockCaptureDoesNotCreateNtpEpoch(ctx);
    testRuntimeRollbackAndLifecycleCleanup(ctx);
}
