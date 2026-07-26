#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"

#include <chrono>
#include <array>
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

std::unique_ptr<MediaChannel> makePreparationReservationChannel(
    std::uint32_t identity)
{
    MediaEdge edge;
    edge.id = MediaEdgeId{identity};
    edge.from = {MediaNodeId{identity}, MediaPortId{1}};
    edge.to = {MediaNodeId{identity + 100}, MediaPortId{1}};
    edge.streamKind = MediaStreamKind::Video;
    edge.edgeKind = MediaEdgeKind::SoftwareFrame;
    edge.payloadKind = MediaPayloadKind::Frame;
    edge.policy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    return std::make_unique<MediaChannel>(MediaChannelId{identity}, edge);
}

std::optional<MediaReservedOutputTransaction> reservePreparationCoordination(
    MediaChannel& channel)
{
    const std::span<const MediaBufferRef> noBuffers;
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{&channel, noBuffers}};
    auto reserved = MediaReservedOutputTransaction::reserve(
        "Video preparation state test", batches);
    if (!reserved || !reserved.value()) return std::nullopt;
    return std::move(*reserved.value());
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

MediaAvSyncPlan completeTsPlan()
{
    auto plan = completeRtpPlan();
    plan.topology = MediaAvSyncTopology::MpegTsToMpegTs;
    plan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    plan.rtp.reset();
    plan.ts.emplace();
    plan.ts->programNumber = 1;
    plan.ts->programMapPid = 0x100;
    plan.ts->videoPid = 0x101;
    plan.ts->audioPid = 0x102;
    plan.ts->pcrPid = 0x101;
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        ms(100), 0x1B, 0x0F, MediaTsH264InputLayout::LengthPrefixed,
        4, MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{ms(20), ms(100), ms(5), 1, 90'000},
        ms(500), 188, MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024});
    plan.ts->outputMux = std::move(mux).value();
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
        graph.setNodeOption(
            sequencer,
            "activated_startup_release_sequencer.output_lead_ns",
            "100000000");
    }
    return graph;
}

MediaGraph graphWithRtpOutputReference(
    const std::optional<std::string>& group,
    std::size_t senderCount = 2,
    bool includePublisher = true,
    bool includeSenderGroups = true)
{
    MediaGraph graph = graphWithGroupReference(group);
    for (std::size_t index = 0; index < senderCount; ++index) {
        const auto sender = graph.addNode(
            MediaNodeKind::ScheduledRtpSender,
            "scheduled-rtp-sender-" + std::to_string(index));
        if (group && includeSenderGroups) {
            graph.setNodeOption(
                sender, "scheduled_rtp.sync_group", *group);
        }
    }
    if (includePublisher) {
        graph.addNode(MediaNodeKind::DualMediaSdpPublisher,
                      "dual-media-sdp-publisher");
    }
    return graph;
}

MediaGraph graphWithTsOutputReference(
    const std::optional<std::string>& group)
{
    MediaGraph graph = graphWithGroupReference(group);
    const auto adapter = graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        "scheduled-ts-adapter");
    const auto source = graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource,
        "project-mpeg-ts-plan-source");
    if (group) {
        graph.setNodeOption(
            adapter, "scheduled_ts_adapter.sync_group", *group);
        graph.setNodeOption(
            source, "project_mpeg_ts_plan.sync_group", *group);
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

MediaAvGenerationTransitionPlan schedulerOnlyTransitionPlan()
{
    return MediaAvGenerationTransitionPlan{
        {{MediaAvGenerationParticipant::Scheduler,
          {"scheduler_generation_state"}}},
        ms(1'000),
        ms(500)};
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
    executable.graph = graphWithRtpOutputReference(nodeGroup);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey(std::move(bindingGroup)), std::move(plan),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    return executable;
}

MediaRealtimeExecutableGraph componentExecutableWith(
    MediaAvGenerationTransitionPlan transition)
{
    MediaRealtimeExecutableGraph executable;
    executable.graph = graphWithGroupReference(
        std::string("realtime.av"));
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        std::move(transition),
        MediaAvSyncBindingAssemblyMode::ComponentCore});
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

    for (const MediaNodeKind legacyAuthority : {
             MediaNodeKind::RtpMux,
             MediaNodeKind::RtpOutput,
             MediaNodeKind::SdpWriter,
             MediaNodeKind::PacketNormalize,
             MediaNodeKind::VideoTimestamp,
             MediaNodeKind::PacketStartGate}) {
        auto legacyProduction = executableWith(completeRtpPlan());
        legacyProduction.graph.addNode(
            legacyAuthority, "legacy-production-authority");
        const auto rejected =
            MediaGraphRuntimeCompiler::validateBindings(legacyProduction);
        EXPECT_FALSE(ctx, rejected);
        if (!rejected) {
            EXPECT_TRUE(
                ctx,
                rejected.error().message.find(
                    "rejects legacy output, timestamp, and startup authorities") !=
                    std::string::npos);
        }
    }

    MediaRealtimeExecutableGraph missingProtocolOutput;
    missingProtocolOutput.graph = graphWithGroupReference(
        std::string("realtime.av"));
    missingProtocolOutput.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(missingProtocolOutput));

    MediaRealtimeExecutableGraph missingRtpPublisher;
    missingRtpPublisher.graph = graphWithRtpOutputReference(
        std::string("realtime.av"), 2, false);
    missingRtpPublisher.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(missingRtpPublisher));

    MediaRealtimeExecutableGraph missingRtpSenderGroups;
    missingRtpSenderGroups.graph = graphWithRtpOutputReference(
        std::string("realtime.av"), 2, true, false);
    missingRtpSenderGroups.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(missingRtpSenderGroups));

    MediaRealtimeExecutableGraph duplicateRtpSender;
    duplicateRtpSender.graph = graphWithRtpOutputReference(
        std::string("realtime.av"), 3, true);
    duplicateRtpSender.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});

    MediaRealtimeExecutableGraph componentCore;
    componentCore.graph = graphWithGroupReference(std::string("realtime.av"));
    componentCore.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ComponentCore});
    EXPECT_TRUE(ctx,
                MediaGraphRuntimeCompiler::validateBindings(componentCore));

    MediaRealtimeExecutableGraph invalidAssemblyMode;
    invalidAssemblyMode.graph = graphWithGroupReference(
        std::string("realtime.av"));
    invalidAssemblyMode.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        static_cast<MediaAvSyncBindingAssemblyMode>(255)});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(invalidAssemblyMode));

    MediaRealtimeExecutableGraph componentWithProtocolOutput;
    componentWithProtocolOutput.graph = graphWithRtpOutputReference(
        std::string("realtime.av"));
    componentWithProtocolOutput.avSyncBinding.emplace(
        MediaAvSyncRuntimeBinding{
            MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
            completeTransitionPlan(),
            MediaAvSyncBindingAssemblyMode::ComponentCore});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(
            componentWithProtocolOutput));
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(duplicateRtpSender));

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

    auto scheduledTsAdapter = executableWith(completeRtpPlan());
    const auto adapterId = scheduledTsAdapter.graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter, "scheduled-ts-adapter");
    scheduledTsAdapter.graph.setNodeOption(
        adapterId, "scheduled_ts_adapter.sync_group", "realtime.av");
    EXPECT_FALSE(ctx,
                MediaGraphRuntimeCompiler::validateBindings(
                    scheduledTsAdapter));

    auto wrongKindExactKey = executableWith(completeRtpPlan());
    const auto wrongKindId = wrongKindExactKey.graph.addNode(
        MediaNodeKind::Demux, "wrong-kind-scheduled-ts-adapter");
    wrongKindExactKey.graph.setNodeOption(
        wrongKindId, "scheduled_ts_adapter.sync_group", "realtime.av");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     wrongKindExactKey));

    auto rightKindWrongKey = executableWith(completeRtpPlan());
    const auto wrongKeyId = rightKindWrongKey.graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        "wrong-key-scheduled-ts-adapter");
    rightKindWrongKey.graph.setNodeOption(
        wrongKeyId, "runtime.sync_group", "realtime.av");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     rightKindWrongKey));

    auto scheduledTsGroupMismatch = executableWith(completeRtpPlan());
    const auto mismatchId = scheduledTsGroupMismatch.graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        "mismatched-scheduled-ts-adapter");
    scheduledTsGroupMismatch.graph.setNodeOption(
        mismatchId, "scheduled_ts_adapter.sync_group", "another.group");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     scheduledTsGroupMismatch));

    auto projectTsPlanSource = executableWith(completeRtpPlan());
    const auto planSourceId = projectTsPlanSource.graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource, "project-ts-plan-source");
    const auto pairedAdapterId = projectTsPlanSource.graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        "project-ts-scheduled-adapter");
    projectTsPlanSource.graph.setNodeOption(
        planSourceId, "project_mpeg_ts_plan.sync_group", "realtime.av");
    projectTsPlanSource.graph.setNodeOption(
        pairedAdapterId, "scheduled_ts_adapter.sync_group", "realtime.av");
    EXPECT_FALSE(ctx,
                MediaGraphRuntimeCompiler::validateBindings(
                    projectTsPlanSource));

    MediaRealtimeExecutableGraph matchingTs;
    matchingTs.graph = graphWithTsOutputReference(
        std::string("realtime.av"));
    matchingTs.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeTsPlan(),
        completeTransitionPlan(MediaAvSyncOutputAdapterKind::ProjectMpegTs),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_TRUE(ctx,
                MediaGraphRuntimeCompiler::validateBindings(matchingTs));

    MediaRealtimeExecutableGraph tsWithLegacyTimestamp;
    tsWithLegacyTimestamp.graph = graphWithTsOutputReference(
        std::string("realtime.av"));
    tsWithLegacyTimestamp.graph.addNode(
        MediaNodeKind::VideoTimestamp, "legacy-ts-timestamp-authority");
    tsWithLegacyTimestamp.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeTsPlan(),
        completeTransitionPlan(MediaAvSyncOutputAdapterKind::ProjectMpegTs),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_FALSE(
        ctx,
        MediaGraphRuntimeCompiler::validateBindings(tsWithLegacyTimestamp));

    MediaRealtimeExecutableGraph tsWithRtpOutput;
    tsWithRtpOutput.graph = graphWithRtpOutputReference(
        std::string("realtime.av"));
    tsWithRtpOutput.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("realtime.av"), completeTsPlan(),
        completeTransitionPlan(MediaAvSyncOutputAdapterKind::ProjectMpegTs),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput});
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(tsWithRtpOutput));

    auto planSourceWrongKind = executableWith(completeRtpPlan());
    const auto planSourceWrongKindId = planSourceWrongKind.graph.addNode(
        MediaNodeKind::Demux, "wrong-kind-project-ts-plan-source");
    planSourceWrongKind.graph.setNodeOption(
        planSourceWrongKindId, "project_mpeg_ts_plan.sync_group",
        "realtime.av");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     planSourceWrongKind));

    auto planSourceWrongKey = executableWith(completeRtpPlan());
    const auto planSourceWrongKeyId = planSourceWrongKey.graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource,
        "wrong-key-project-ts-plan-source");
    planSourceWrongKey.graph.setNodeOption(
        planSourceWrongKeyId, "runtime.sync_group", "realtime.av");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     planSourceWrongKey));

    auto planSourceGroupMismatch = executableWith(completeRtpPlan());
    const auto planSourceMismatchId = planSourceGroupMismatch.graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource,
        "mismatched-project-ts-plan-source");
    planSourceGroupMismatch.graph.setNodeOption(
        planSourceMismatchId, "project_mpeg_ts_plan.sync_group",
        "another.group");
    EXPECT_FALSE(ctx,
                 MediaGraphRuntimeCompiler::validateBindings(
                     planSourceGroupMismatch));
}

void testBootstrapCapturesOneClockAndOneRtpEpoch(TestContext& ctx)
{
    const MediaAvSyncRuntimeBinding binding{
        MediaAvSyncGroupKey("realtime.av"), completeRtpPlan(),
        completeTransitionPlan(),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput};
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
    executable.graph = graphWithRtpOutputReference(
        std::string("realtime.av"));
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
    const MediaAvSyncRuntimeBinding binding{
        MediaAvSyncGroupKey("realtime.av"), completeTsPlan(),
        completeTransitionPlan(MediaAvSyncOutputAdapterKind::ProjectMpegTs),
        MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput};
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
    MediaGraph graph;
    const auto lifecycleTarget = graph.addNode(
        MediaNodeKind::Finalize, "lifecycle-target");
    EXPECT_TRUE(ctx, runtime.compile(std::move(graph)));
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Compiled);

    auto transitionService = MediaAvEpochTransitionService::create(
        schedulerOnlyTransitionPlan());
    EXPECT_TRUE(ctx, transitionService);
    if (!transitionService) return;
    DeterministicClockSource clockSource;
    auto clocks = clockSource.capture(true);
    EXPECT_TRUE(ctx, clocks);
    if (!clocks) return;
    const MediaAvSyncGroupKey groupKey("realtime.av");
    EXPECT_TRUE(ctx, runtime.context().registerAvSyncGroup(
                         groupKey, completeRtpPlan(),
                         clocks.value().masterClock,
                         clocks.value().sharedNtpEpoch,
                         transitionService.value()));
    auto group = runtime.context().findAvSyncGroup(
        groupKey);
    EXPECT_TRUE(ctx, group != nullptr);
    auto nodeState = std::make_shared<DeadlineNodeState>();
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
                         std::make_unique<DeadlineRuntimeNode>(
                             lifecycleTarget,
                             groupKey,
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
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(groupKey) == nullptr);
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

void testDefaultRegistrationGatesAvSyncRuntimeReadiness(TestContext& ctx)
{
    MediaGraphRuntime complete;
    EXPECT_TRUE(ctx, complete.compile(componentExecutableWith(
                         schedulerOnlyTransitionPlan())));
    EXPECT_EQ(ctx, complete.state(),
              MediaGraphRuntimeState::DefaultRegistrationPending);
    EXPECT_FALSE(ctx, complete.compiled());
    EXPECT_FALSE(ctx, complete.run());
    EXPECT_FALSE(ctx, complete.startThreaded());
    EXPECT_EQ(ctx, complete.state(),
              MediaGraphRuntimeState::DefaultRegistrationPending);
    EXPECT_TRUE(ctx, complete.registerDefaultRuntimeNodes());
    EXPECT_EQ(ctx, complete.state(), MediaGraphRuntimeState::Compiled);
    EXPECT_TRUE(ctx, complete.compiled());

    auto missingPlan = schedulerOnlyTransitionPlan();
    missingPlan.participants.front().requiredChildren.push_back(
        "missing_scheduler_generation_state");
    MediaGraphRuntime missing;
    EXPECT_TRUE(ctx, missing.compile(componentExecutableWith(
                         std::move(missingPlan))));
    EXPECT_EQ(ctx, missing.state(),
              MediaGraphRuntimeState::DefaultRegistrationPending);
    EXPECT_FALSE(ctx, missing.compiled());
    EXPECT_FALSE(ctx, missing.run());
    EXPECT_FALSE(ctx, missing.startThreaded());
    auto group = missing.context().findAvSyncGroup(
        MediaAvSyncGroupKey("realtime.av"));
    EXPECT_TRUE(ctx, group != nullptr);
    const auto registered = missing.registerDefaultRuntimeNodes();
    EXPECT_FALSE(ctx, registered);
    if (!registered) {
        EXPECT_EQ(ctx, registered.error().code,
                  ::media::ErrorCode::InvalidArgument);
        EXPECT_EQ(
            ctx, registered.error().message,
            std::string(
                "Generation participant can seal exactly once with its complete child set"));
    }
    EXPECT_EQ(ctx, missing.state(), MediaGraphRuntimeState::Aborted);
    EXPECT_FALSE(ctx, missing.compiled());
    EXPECT_FALSE(ctx, missing.run());
    EXPECT_FALSE(ctx, missing.startThreaded());
    EXPECT_TRUE(ctx, group && group->lifecycleState() ==
                         MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_TRUE(ctx, missing.context().findAvSyncGroup(
                         MediaAvSyncGroupKey("realtime.av")) == nullptr);
}

void testVideoPreparationStateEnforcesStrictTransitions(TestContext& ctx)
{
    auto state = MediaAvStartupVideoPreparationState::create(
        MediaAvSyncGroupKey("preparation-state"));
    EXPECT_TRUE(ctx, state);
    if (!state) return;

    constexpr std::uint64_t generation = 7;
    constexpr std::uint64_t releaseIdentity = 41;
    EXPECT_EQ(ctx, state.value()->snapshot().phase,
              MediaAvStartupVideoPreparationPhase::Awaiting);
    EXPECT_FALSE(ctx, state.value()->authorizeRelease(
                          generation, releaseIdentity,
                          [] { return ::media::Status::success(); }));
    EXPECT_TRUE(ctx, state.value()->begin(
                         generation, releaseIdentity, 3));
    EXPECT_FALSE(ctx, state.value()->begin(
                          generation, releaseIdentity, 3));
    auto first = state.value()->reserveNextVideoUnit(
        generation, releaseIdentity);
    EXPECT_TRUE(ctx, first &&
                         first.value().kind ==
                             MediaAvStartupVideoReservationKind::Reserved &&
                         first.value().index && *first.value().index == 0);
    auto sameReservation = state.value()->reserveNextVideoUnit(
        generation, releaseIdentity);
    EXPECT_TRUE(ctx, sameReservation &&
                         sameReservation.value().kind ==
                             MediaAvStartupVideoReservationKind::Reserved &&
                         sameReservation.value().index &&
                         *sameReservation.value().index == 0);
    auto filterChannel = makePreparationReservationChannel(901);
    auto extractorChannel = makePreparationReservationChannel(902);
    auto filterReservation = reservePreparationCoordination(*filterChannel);
    auto extractorReservation = reservePreparationCoordination(*extractorChannel);
    EXPECT_TRUE(ctx, filterReservation && extractorReservation);
    if (!filterReservation || !extractorReservation) return;
    EXPECT_FALSE(ctx, state.value()->markFilterReady(
                          generation + 1, releaseIdentity,
                          filterReservation->handle()));
    EXPECT_TRUE(ctx, state.value()->markFilterReady(
                         generation, releaseIdentity,
                         filterReservation->handle()));
    EXPECT_TRUE(ctx, state.value()->commitVideoUnit(
                         generation, releaseIdentity, 0));
    auto afterReady = state.value()->reserveNextVideoUnit(
        generation, releaseIdentity);
    EXPECT_TRUE(ctx, afterReady &&
                         afterReady.value().kind ==
                             MediaAvStartupVideoReservationKind::NoReservation &&
                         !afterReady.value().index);
    EXPECT_EQ(ctx, state.value()->snapshot().committedVideoUnits,
              std::size_t{1});
    EXPECT_TRUE(ctx, state.value()->registerExtractorOutputs(
                         generation, releaseIdentity,
                         extractorReservation->handle()));
    const MediaPlaybackEpoch anchoredEpoch{
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(20), generation};
    const MediaAudioPlaybackOrigin anchoredOrigin{
        generation, anchoredEpoch.sourceStart, anchoredEpoch.masterRelease,
        0, 48'000};
    EXPECT_TRUE(ctx, state.value()->publishInitialAnchor(
                         generation, releaseIdentity,
                         anchoredEpoch, anchoredOrigin));
    EXPECT_TRUE(ctx, state.value()->acknowledgeExtractorReanchor(
                         generation, releaseIdentity));
    EXPECT_TRUE(ctx, state.value()->authorizeRelease(
                         generation, releaseIdentity,
                         [] { return ::media::Status::success(); }));
    EXPECT_EQ(ctx, state.value()->snapshot().phase,
              MediaAvStartupVideoPreparationPhase::ReleaseCommitted);
}

void testVideoPreparationStateNotifiesWaitingPeersAfterUnlock(
    TestContext& ctx)
{
    auto state = MediaAvStartupVideoPreparationState::create(
        MediaAvSyncGroupKey("preparation-wakeup"));
    EXPECT_TRUE(ctx, state);
    if (!state) return;
    auto sequencerWakeup = std::make_shared<MediaNodeWakeup>();
    auto filterWakeup = std::make_shared<MediaNodeWakeup>();
    auto extractorWakeup = std::make_shared<MediaNodeWakeup>();
    EXPECT_TRUE(ctx, state.value()->bindSequencerWakeup(sequencerWakeup));
    EXPECT_TRUE(ctx, state.value()->bindFilterWakeup(filterWakeup));
    EXPECT_TRUE(ctx, state.value()->bindExtractorWakeup(extractorWakeup));
    const auto sequencerSequence = sequencerWakeup->sequence();
    const auto filterSequence = filterWakeup->sequence();
    const auto extractorSequence = extractorWakeup->sequence();
    EXPECT_TRUE(ctx, state.value()->begin(11, 55, 1));
    auto filterChannel = makePreparationReservationChannel(903);
    auto extractorChannel = makePreparationReservationChannel(904);
    auto filterReservation = reservePreparationCoordination(*filterChannel);
    auto extractorReservation = reservePreparationCoordination(*extractorChannel);
    EXPECT_TRUE(ctx, filterReservation && extractorReservation);
    if (!filterReservation || !extractorReservation) return;
    EXPECT_TRUE(ctx, state.value()->markFilterReady(
                         11, 55, filterReservation->handle()));
    EXPECT_TRUE(ctx, sequencerWakeup->sequence() > sequencerSequence);
    EXPECT_TRUE(ctx, state.value()->registerExtractorOutputs(
                         11, 55, extractorReservation->handle()));
    const MediaPlaybackEpoch anchoredEpoch{
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(20), 11};
    const MediaAudioPlaybackOrigin anchoredOrigin{
        11, anchoredEpoch.sourceStart, anchoredEpoch.masterRelease,
        0, 48'000};
    EXPECT_TRUE(ctx, state.value()->publishInitialAnchor(
                         11, 55, anchoredEpoch, anchoredOrigin));
    EXPECT_TRUE(ctx, state.value()->acknowledgeExtractorReanchor(11, 55));
    EXPECT_TRUE(ctx, state.value()->authorizeRelease(
                         11, 55, [] { return ::media::Status::success(); }));
    EXPECT_TRUE(ctx, filterWakeup->sequence() > filterSequence);
    EXPECT_TRUE(ctx, extractorWakeup->sequence() > extractorSequence);
}

void testVideoPreparationCapabilitiesShareOneStateAndCancelTerminally(
    TestContext& ctx)
{
    auto state = MediaAvStartupVideoPreparationState::create(
        MediaAvSyncGroupKey("preparation-capabilities"));
    EXPECT_TRUE(ctx, state);
    if (!state) return;
    auto extractor = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::ExtractorFeed);
    auto filter = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::FilterReadiness);
    auto sequencer = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::SequencerActivation);
    EXPECT_TRUE(ctx, extractor && filter && sequencer);
    if (!extractor || !filter || !sequencer) return;
    EXPECT_TRUE(ctx, extractor.value().stateIdentity() ==
                         filter.value().stateIdentity());
    EXPECT_TRUE(ctx, filter.value().stateIdentity() ==
                         sequencer.value().stateIdentity());
    EXPECT_TRUE(ctx, extractor.value().begin(9, 77, 1));
    EXPECT_TRUE(ctx, extractor.value().cancel());
    EXPECT_FALSE(ctx, filter.value().markFilterReady(9, 77, {}));
    EXPECT_FALSE(ctx, sequencer.value().authorizeRelease(
                          9, 77, [] { return ::media::Status::success(); }));
}

} // namespace

void runAvSyncRuntimeBootstrapTests(
    media_transcode::test::TestContext& ctx)
{
    testBootstrapRejectsIncompleteBindings(ctx);
    testBootstrapCapturesOneClockAndOneRtpEpoch(ctx);
    testTsClockCaptureDoesNotCreateNtpEpoch(ctx);
    testRuntimeRollbackAndLifecycleCleanup(ctx);
    testDefaultRegistrationGatesAvSyncRuntimeReadiness(ctx);
    testVideoPreparationStateEnforcesStrictTransitions(ctx);
    testVideoPreparationCapabilitiesShareOneStateAndCancelTerminally(ctx);
    testVideoPreparationStateNotifiesWaitingPeersAfterUnlock(ctx);
}
