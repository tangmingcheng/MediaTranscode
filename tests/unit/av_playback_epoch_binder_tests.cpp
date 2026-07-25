#include "common/TestAssert.h"
#include "common/AvSyncRuntimeTestSupport.h"

#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"

#include <concepts>
#include <string>
#include <type_traits>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

template <typename T>
concept HasDirectInitialEpochActivation = requires(
    T& value, MediaPlaybackEpoch epoch, MediaAudioPlaybackOrigin origin) {
    value.publishInitial(epoch, origin);
};

template <typename T>
concept HasDirectNextEpochActivation = requires(
    T& value, MediaPlaybackEpoch epoch, MediaAudioPlaybackOrigin origin) {
    value.publishNext(epoch, origin, std::uint64_t{1});
};

static_assert(!HasDirectInitialEpochActivation<MediaPlaybackEpochBinderNode>);
static_assert(!HasDirectNextEpochActivation<MediaPlaybackEpochBinderNode>);

static_assert(std::move_constructible<MediaPlaybackEpochActivationCapability>);
static_assert(std::is_move_assignable_v<MediaPlaybackEpochActivationCapability>);
static_assert(!std::copy_constructible<MediaPlaybackEpochActivationCapability>);
static_assert(!std::is_copy_assignable_v<MediaPlaybackEpochActivationCapability>);
static_assert(!std::default_initializable<MediaPlaybackEpochActivationCapability>);

template <typename T>
concept HasPublicInitialEpochActivation = requires(
    T& value, MediaPlaybackEpoch playbackEpoch,
    MediaAudioPlaybackOrigin audioOrigin) {
    value.activateInitial(playbackEpoch, audioOrigin);
};

template <typename T>
concept HasPublicLegacyGroupActivation = requires(
    T& value, MediaPlaybackEpoch playbackEpoch) {
    value.activatePlaybackEpoch(playbackEpoch);
};

template <typename T>
concept HasPublicContextActivation = requires(
    T& value, MediaAvSyncGroupKey key, MediaPlaybackEpoch playbackEpoch) {
    value.activatePlaybackEpoch(key, playbackEpoch);
};

static_assert(!HasPublicInitialEpochActivation<MediaAvEpochTransitionService>);
static_assert(!HasPublicLegacyGroupActivation<MediaAvSyncGroupRuntime>);
static_assert(!HasPublicContextActivation<MediaGraphExecutionContext>);

class OccupiedRuntimeNode final : public MediaRuntimeNode {
public:
    explicit OccupiedRuntimeNode(MediaNodeId id) : m_id(id) {}
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

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaAvSyncPlan completePlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "binder-test";
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
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
        ms(1'000), ms(500));
}

struct BinderExecutable final {
    MediaRealtimeExecutableGraph executable;
    MediaNodeId binder;
    MediaNodeId sequencer;
};

BinderExecutable makeExecutable(std::string binderGroup = "binder-group")
{
    BinderExecutable fixture;
    const auto scheduler = fixture.executable.graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    fixture.binder = fixture.executable.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    fixture.executable.graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", "binder-group");
    fixture.executable.graph.setNodeOption(
        fixture.binder, "playback_epoch_binder.sync_group",
        std::move(binderGroup));
    media_transcode::test::addPlaybackEpochReleaseBoundary(
        fixture.executable.graph, fixture.binder);
    for (const MediaNode& node : fixture.executable.graph.nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer) {
            fixture.sequencer = node.id;
            break;
        }
    }
    fixture.executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("binder-group"), completePlan(),
        transitionPlan(), MediaAvSyncBindingAssemblyMode::ComponentCore});
    return fixture;
}

MediaPlaybackEpoch epoch(std::uint64_t generation);
MediaAudioPlaybackOrigin origin(std::uint64_t generation);

void testReleasePipelineActivatesInitialOnlyAndRejectsBinderGroupMismatch(
    TestContext& ctx)
{
    auto fixture = makeExecutable();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime.scheduler().findNode(fixture.binder));
    auto group = runtime.context().findAvSyncGroup(MediaAvSyncGroupKey("binder-group"));
    MediaChannel* input = runtime.context().findInputChannel(fixture.binder, "release");
    EXPECT_TRUE(ctx, binder != nullptr && group != nullptr && input != nullptr);
    if (!binder || !group || !input) return;
    auto payload = [] { return makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(1)); };
    auto initial = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("binder-group"),
        MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch(1), origin(1), {{payload(), 0}}, {{payload(), 0}});
    EXPECT_TRUE(ctx, initial);
    EXPECT_TRUE(ctx, media_transcode::test::activateInitialThroughRelease(
        runtime, fixture.binder, MediaAvSyncGroupKey("binder-group"),
        epoch(1)));
    const auto activated = group->epochTransitionSnapshot();
    EXPECT_EQ(ctx, activated.readiness, MediaAvGenerationReadiness::Locked);

    auto active = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("binder-group"),
        MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch(1), origin(1), {{payload(), 0}}, {});
    EXPECT_TRUE(ctx, active);
    EXPECT_TRUE(ctx, input->push(active.value()));
    EXPECT_TRUE(ctx, binder->process(runtime.context()));
    auto* sequencer = dynamic_cast<MediaActivatedStartupReleaseSequencerNode*>(
        runtime.scheduler().findNode(fixture.sequencer));
    EXPECT_TRUE(ctx, sequencer != nullptr);
    if (!sequencer) return;
    EXPECT_TRUE(ctx, sequencer->process(runtime.context()));
    const auto afterPassThrough = group->epochTransitionSnapshot();
    EXPECT_EQ(ctx, afterPassThrough.playbackEpoch, activated.playbackEpoch);
    EXPECT_EQ(ctx, afterPassThrough.readiness, MediaAvGenerationReadiness::Locked);

    auto mismatch = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("other-group"),
        MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch(1), origin(1), {}, {{payload(), 0}});
    EXPECT_TRUE(ctx, mismatch);
    EXPECT_TRUE(ctx, input->push(mismatch.value()));
    EXPECT_FALSE(ctx, binder->process(runtime.context()));
}

MediaPlaybackEpoch epoch(std::uint64_t generation)
{
    return MediaPlaybackEpoch{ms(10), ms(20), generation};
}

MediaAudioPlaybackOrigin origin(std::uint64_t generation)
{
    return MediaAudioPlaybackOrigin{generation, ms(10), ms(20), 0, 48'000};
}

void testBinderKindIsAppendOnlyAndObservable(TestContext& ctx)
{
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::AvOutputScheduler), 46);
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::PlaybackEpochBinder), 47);
    EXPECT_EQ(ctx, MediaPlaybackEpochBinderNode::staticKind(),
              MediaNodeKind::PlaybackEpochBinder);

    MediaGraph graph;
    graph.addNode(MediaNodeKind::PlaybackEpochBinder, "epoch-binder");
    EXPECT_TRUE(ctx, MediaGraphDump::toText(graph).find("PlaybackEpochBinder") !=
                         std::string::npos);
    EXPECT_EQ(ctx, std::string(mediaGraphDiagnosticNodeKindName(
                       MediaNodeKind::PlaybackEpochBinder)),
              std::string("PlaybackEpochBinder"));
}

void testCompilerIssuesOneCapabilityToOneSequencer(TestContext& ctx)
{
    auto fixture = makeExecutable();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());

    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime.scheduler().findNode(fixture.binder));
    auto* sequencer = dynamic_cast<MediaActivatedStartupReleaseSequencerNode*>(
        runtime.scheduler().findNode(fixture.sequencer));
    EXPECT_TRUE(ctx, binder != nullptr && sequencer != nullptr);
    auto group = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("binder-group"));
    EXPECT_TRUE(ctx, group != nullptr);
    if (!binder || !sequencer || !group) return;

    EXPECT_EQ(ctx, binder->groupKey(), MediaAvSyncGroupKey("binder-group"));
    auto* input = runtime.context().findInputChannel(fixture.binder, "release");
    EXPECT_TRUE(ctx, input != nullptr);
    if (!input) return;
    const auto payload = [] {
        return makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(1));
    };
    auto release = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("binder-group"),
        MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch(1), origin(1), {{payload(), 0}}, {{payload(), 0}});
    EXPECT_TRUE(ctx, release);
    EXPECT_TRUE(ctx, media_transcode::test::activateInitialThroughRelease(
        runtime, fixture.binder, MediaAvSyncGroupKey("binder-group"),
        epoch(1)));
    auto initial = group->epochTransitionSnapshot();
    EXPECT_EQ(ctx, initial.readiness, MediaAvGenerationReadiness::Locked);
    EXPECT_TRUE(ctx, initial.playbackEpoch &&
                         initial.playbackEpoch->generation == 1);
    EXPECT_TRUE(ctx, initial.audioOrigin &&
                         initial.audioOrigin->generation == 1);
    EXPECT_TRUE(ctx, initial.outputPermitted);

    EXPECT_TRUE(ctx, input->push(release.value()));
    EXPECT_TRUE(ctx, binder->process(runtime.context()));
    EXPECT_FALSE(ctx, sequencer->process(runtime.context()));
}

void testCompilerRejectsMissingDuplicateAndMismatchedBinder(TestContext& ctx)
{
    auto missing = makeExecutable();
    auto graph = MediaGraph{};
    const auto scheduler = graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    graph.setNodeOption(scheduler, "av_scheduler.sync_group", "binder-group");
    missing.executable.graph = std::move(graph);
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(
                          missing.executable));

    auto duplicate = makeExecutable();
    const auto second = duplicate.executable.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "second-binder");
    duplicate.executable.graph.setNodeOption(
        second, "playback_epoch_binder.sync_group", "binder-group");
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(
                          duplicate.executable));

    auto mismatch = makeExecutable("another-group");
    EXPECT_FALSE(ctx, MediaGraphRuntimeCompiler::validateBindings(
                          mismatch.executable));
}

void testCompileRollbackPreservesActiveRuntimeForBinderCardinalityErrors(
    TestContext& ctx)
{
    auto valid = makeExecutable();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(valid.executable)));
    const MediaGraph* activeGraph = runtime.graph();
    auto activeGroup = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("binder-group"));
    const auto activeState = runtime.state();

    auto missing = makeExecutable();
    MediaGraph missingGraph;
    const auto scheduler = missingGraph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    missingGraph.setNodeOption(
        scheduler, "av_scheduler.sync_group", "binder-group");
    missing.executable.graph = std::move(missingGraph);
    EXPECT_FALSE(ctx, runtime.compile(std::move(missing.executable)));
    EXPECT_TRUE(ctx, runtime.graph() == activeGraph);
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(
                         MediaAvSyncGroupKey("binder-group")) == activeGroup);
    EXPECT_EQ(ctx, runtime.state(), activeState);

    auto duplicate = makeExecutable();
    const auto extra = duplicate.executable.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "duplicate-binder");
    duplicate.executable.graph.setNodeOption(
        extra, "playback_epoch_binder.sync_group", "binder-group");
    EXPECT_FALSE(ctx, runtime.compile(std::move(duplicate.executable)));
    EXPECT_TRUE(ctx, runtime.graph() == activeGraph);
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(
                         MediaAvSyncGroupKey("binder-group")) == activeGroup);
    EXPECT_EQ(ctx, runtime.state(), activeState);
}

void testDefaultRegistrationFailureDoesNotPartiallyPublishBinder(
    TestContext& ctx)
{
    auto fixture = makeExecutable();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
                         std::make_unique<OccupiedRuntimeNode>(fixture.sequencer)));
    EXPECT_FALSE(ctx, runtime.registerDefaultRuntimeNodes());
    EXPECT_TRUE(ctx, runtime.scheduler().findNode(MediaNodeId{1}) == nullptr);
}

void testRejectedSchedulerBatchPreservesPublishedState(TestContext& ctx)
{
    MediaGraphScheduler scheduler;
    auto existing = std::make_unique<OccupiedRuntimeNode>(MediaNodeId{80});
    auto* existingAddress = existing.get();
    EXPECT_TRUE(ctx, scheduler.registerNode(std::move(existing)));

    std::vector<std::unique_ptr<MediaRuntimeNode>> batch;
    batch.push_back(std::make_unique<OccupiedRuntimeNode>(MediaNodeId{81}));
    batch.push_back(std::make_unique<OccupiedRuntimeNode>(MediaNodeId{80}));
    EXPECT_FALSE(ctx, scheduler.registerNodes(std::move(batch)));
    EXPECT_TRUE(ctx, scheduler.findNode(MediaNodeId{80}) == existingAddress);
    EXPECT_TRUE(ctx, scheduler.findNode(MediaNodeId{81}) == nullptr);
}

void testLifecyclePoisonsIssuedAuthority(TestContext& ctx)
{
    auto fixture = makeExecutable();
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
    auto group = runtime.context().findAvSyncGroup(
        MediaAvSyncGroupKey("binder-group"));
    EXPECT_TRUE(ctx, group != nullptr);
    runtime.abort();
    if (group) {
        EXPECT_TRUE(ctx, group->epochTransitionSnapshot().poisoned);
    }
    runtime.reset();
    EXPECT_TRUE(ctx, runtime.context().findAvSyncGroup(
                         MediaAvSyncGroupKey("binder-group")) == nullptr);
}

} // namespace

int main()
{
    TestContext ctx;
    testBinderKindIsAppendOnlyAndObservable(ctx);
    testCompilerIssuesOneCapabilityToOneSequencer(ctx);
    testCompilerRejectsMissingDuplicateAndMismatchedBinder(ctx);
    testCompileRollbackPreservesActiveRuntimeForBinderCardinalityErrors(ctx);
    testDefaultRegistrationFailureDoesNotPartiallyPublishBinder(ctx);
    testRejectedSchedulerBatchPreservesPublishedState(ctx);
    testLifecyclePoisonsIssuedAuthority(ctx);
    testReleasePipelineActivatesInitialOnlyAndRejectsBinderGroupMismatch(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
