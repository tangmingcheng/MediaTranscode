#include "common/TestAssert.h"

#include "internal/graph/nodes/sync/MediaAvStartupClockNode.h"
#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"
#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"
#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

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
    MediaNodeId scheduler;
    MediaNodeId binder;
    MediaNodeId source;
    MediaNodeId sequencer;
    MediaNodeId firstActivatedSink;
    MediaNodeId secondActivatedSink;
    MediaNodeId releaseSink;
    MediaNodeId preparationSink = MediaNodeId::invalid();
    std::shared_ptr<MediaAvStartupVideoPreparationState> preparationState;
    std::string firstActivatedPort = "in";
};

BinderFixture binderFixture(bool threadedLifecycleTarget = false,
                            bool preactivation = false)
{
    BinderFixture fixture;
    fixture.scheduler = fixture.executable.graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    fixture.binder = fixture.executable.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    fixture.source = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "source");
    fixture.sequencer = fixture.executable.graph.addNode(
        MediaNodeKind::ActivatedStartupReleaseSequencer, "sequencer");
    fixture.firstActivatedSink = threadedLifecycleTarget
        ? fixture.scheduler
        : fixture.executable.graph.addNode(
              MediaNodeKind::DebugDump, "first-activated-sink");
    if (threadedLifecycleTarget) fixture.firstActivatedPort = "video";
    fixture.secondActivatedSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "second-activated-sink");
    fixture.releaseSink = fixture.executable.graph.addNode(
        MediaNodeKind::DebugDump, "release-sink");
    if (preactivation) {
        fixture.preparationSink = fixture.executable.graph.addNode(
            MediaNodeKind::DebugDump, "preparation-sink");
        auto state = MediaAvStartupVideoPreparationState::create(
            MediaAvSyncGroupKey("task4-group"));
        if (state) fixture.preparationState = std::move(state).value();
    }
    fixture.executable.graph.setNodeOption(
        fixture.scheduler, "av_scheduler.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.scheduler, "av_scheduler.transport_lead_ns", "0");
    fixture.executable.graph.setNodeOption(
        fixture.binder, "playback_epoch_binder.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.sequencer,
        "activated_startup_release_sequencer.sync_group", "task4-group");
    fixture.executable.graph.setNodeOption(
        fixture.sequencer,
        "activated_startup_release_sequencer.output_lead_ns", "100000000");
    fixture.executable.graph.addOutputPort(
        fixture.source, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addInputPort(
        fixture.binder, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    fixture.executable.graph.addOutputPort(
        fixture.binder, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    if (preactivation) {
        fixture.executable.graph.addOutputPort(
            fixture.binder, "preparation", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        fixture.executable.graph.addInputPort(
            fixture.preparationSink, "in", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    }
    fixture.executable.graph.addInputPort(
        fixture.sequencer, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto activatedStream = threadedLifecycleTarget
        ? MediaStreamKind::Video
        : MediaStreamKind::Metadata;
    fixture.executable.graph.addOutputPort(
        fixture.sequencer, "activated", activatedStream,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    fixture.executable.graph.addOutputPort(
        fixture.sequencer, "bound_release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    if (!threadedLifecycleTarget) {
        fixture.executable.graph.addInputPort(
            fixture.firstActivatedSink, fixture.firstActivatedPort,
            activatedStream, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent);
    } else {
        fixture.executable.graph.addInputPort(
            fixture.scheduler, "video", MediaStreamKind::Video,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    }
    fixture.executable.graph.addInputPort(
        fixture.secondActivatedSink, "in", activatedStream,
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
    if (preactivation) {
        fixture.executable.graph.connect(
            fixture.binder, "preparation", fixture.preparationSink, "in",
            "preparation", one);
    }
    fixture.executable.graph.connect(
        fixture.sequencer, "activated", fixture.firstActivatedSink,
        fixture.firstActivatedPort,
        "first-activated", one);
    fixture.executable.graph.connect(
        fixture.sequencer, "activated", fixture.secondActivatedSink, "in",
        "second-activated", one);
    fixture.executable.graph.connect(
        fixture.sequencer, "bound_release", fixture.releaseSink, "in", "bound", one);
    if (threadedLifecycleTarget) {
        const auto audioSource = fixture.executable.graph.addNode(
            MediaNodeKind::DebugDump, "scheduler-audio-source");
        const auto schedulerSink = fixture.executable.graph.addNode(
            MediaNodeKind::DebugDump, "scheduler-output-sink");
        fixture.executable.graph.addInputPort(
            fixture.scheduler, "audio", MediaStreamKind::Audio,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        fixture.executable.graph.addOutputPort(
            audioSource, "audio", MediaStreamKind::Audio,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        fixture.executable.graph.addOutputPort(
            fixture.scheduler, "out", MediaStreamKind::Video,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        fixture.executable.graph.addInputPort(
            schedulerSink, "in", MediaStreamKind::Video,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        fixture.executable.graph.connect(
            audioSource, "audio", fixture.scheduler, "audio",
            "scheduler-audio", one);
        fixture.executable.graph.connect(
            fixture.scheduler, "out", schedulerSink, "in",
            "scheduler-output", one);
    }
    fixture.executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        MediaAvSyncGroupKey("task4-group"), completePlan(), transitionPlan(),
        MediaAvSyncBindingAssemblyMode::ComponentCore,
        fixture.preparationState});
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
        EXPECT_TRUE(ctx, transaction->payload() == expectedRelease);
        EXPECT_TRUE(ctx, transaction->release() != nullptr);
        if (transaction->release()) {
            EXPECT_EQ(ctx, transaction->release()->groupKey(),
                      MediaAvSyncGroupKey("task4-group"));
            EXPECT_EQ(ctx, transaction->release()->epoch(), epoch());
        }
    }
}

void testPreparationPrefixIsNotReleasedTwiceAfterActivation(TestContext& ctx)
{
    MediaGraph graph;
    const auto preparationSource = graph.addNode(
        MediaNodeKind::DebugDump, "preparation-source");
    const auto boundSource = graph.addNode(MediaNodeKind::DebugDump, "bound-source");
    const auto extractorId = graph.addNode(
        MediaNodeKind::AvBoundReleaseExtractor, "extractor");
    const auto videoSink = graph.addNode(MediaNodeKind::DebugDump, "video-sink");
    const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio-sink");
    graph.addOutputPort(preparationSource, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(boundSource, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(extractorId, "preparation", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(extractorId, "bound_release", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(extractorId, "video", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(extractorId, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(videoSink, "in", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(audioSink, "in", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    const auto videoPolicy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    graph.connect(preparationSource, "out", extractorId, "preparation",
                  "preparation", policy);
    graph.connect(boundSource, "out", extractorId, "bound_release",
                  "bound", policy);
    graph.connect(extractorId, "video", videoSink, "in", "video", videoPolicy);
    graph.connect(extractorId, "audio", audioSink, "in", "audio", policy);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));

    auto state = MediaAvStartupVideoPreparationState::create(
        MediaAvSyncGroupKey("task4-group"));
    EXPECT_TRUE(ctx, state);
    if (!state) return;
    auto feed = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::ExtractorFeed);
    EXPECT_TRUE(ctx, feed);
    if (!feed) return;
    MediaAvBoundReleaseExtractorNode extractor(
        extractorId, std::move(feed).value());
    EXPECT_TRUE(ctx, extractor.start(execution));

    const auto unit = [] { return makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(1)); };
    const MediaBufferRef video0 = unit();
    const MediaBufferRef video1 = unit();
    const MediaBufferRef video2 = unit();
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    packet->size = 1;
    auto rawAudio = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Audio, std::nullopt);
    EXPECT_TRUE(ctx, rawAudio);
    if (!rawAudio) return;
    auto audioLineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            ms(10), std::nullopt, ms(10),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            "preparation-audio", MediaSourceAccessUnitSequence(1),
            MediaTimeMappingConfidence::Locked, epoch().generation});
    auto canonicalAudio = MediaCanonicalAccessUnitBuffer::create(
        rawAudio.value(), std::move(audioLineage));
    EXPECT_TRUE(ctx, canonicalAudio);
    if (!canonicalAudio) return;
    const MediaBufferRef audio0 = std::move(canonicalAudio).value();
    auto releaseResult = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("task4-group"),
        MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch(), origin(),
        {{video0, 0}, {video1, 0}, {video2, 0}}, {{audio0, 0}});
    EXPECT_TRUE(ctx, releaseResult);
    if (!releaseResult) return;
    auto transactionResult = MediaStartupReleaseTransactionBuffer::create(
        releaseResult.value());
    EXPECT_TRUE(ctx, transactionResult);
    if (!transactionResult) return;
    auto* preparationInput = execution.findInputChannel(
        extractorId, "preparation");
    auto* boundInput = execution.findInputChannel(extractorId, "bound_release");
    auto* videoOutput = execution.findInputChannel(videoSink, "in");
    auto* audioOutput = execution.findInputChannel(audioSink, "in");
    EXPECT_TRUE(ctx, preparationInput && boundInput && videoOutput && audioOutput);
    if (!preparationInput || !boundInput || !videoOutput || !audioOutput) return;
    EXPECT_TRUE(ctx, preparationInput->push(transactionResult.value()));
    EXPECT_TRUE(ctx, extractor.process(execution));
    MediaBufferRef observed;
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == video0);
    EXPECT_FALSE(ctx, audioOutput->tryPop(observed));
    const auto* transaction = dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
        transactionResult.value().get());
    EXPECT_TRUE(ctx, transaction != nullptr);
    if (!transaction) return;
    const std::span<const MediaBufferRef> noPreparedOutputs;
    const std::array<MediaAtomicOutputBatch, 1> readinessBatches{
        MediaAtomicOutputBatch{videoOutput, noPreparedOutputs}};
    auto filterReservation = MediaReservedOutputTransaction::reserve(
        "Extractor prefix race filter readiness", readinessBatches);
    EXPECT_TRUE(ctx, filterReservation && filterReservation.value());
    if (!filterReservation || !filterReservation.value()) return;

    const MediaBufferRef backpressure = unit();
    EXPECT_TRUE(ctx, videoOutput->push(backpressure));
    const auto blocked = extractor.process(execution);
    EXPECT_TRUE(ctx, blocked &&
                         blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, state.value()->snapshot().committedVideoUnits,
              std::size_t{1});
    EXPECT_TRUE(ctx, state.value()->markFilterReady(
                         epoch().generation, transaction->releaseIdentity(),
                         filterReservation.value()->handle()));
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == backpressure);
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == video1);
    EXPECT_FALSE(ctx, videoOutput->tryPop(observed));
    EXPECT_EQ(ctx, state.value()->snapshot().committedVideoUnits,
              std::size_t{2});
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_TRUE(ctx, state.value()->snapshot().extractorOutputsReserved);
    EXPECT_TRUE(ctx, state.value()->publishInitialAnchor(
                         epoch().generation, transaction->releaseIdentity(),
                         epoch(), origin()));
    EXPECT_TRUE(ctx, state.value()->acknowledgeExtractorReanchor(
                         epoch().generation, transaction->releaseIdentity()));
    EXPECT_TRUE(ctx, state.value()->authorizeRelease(
                         epoch().generation, transaction->releaseIdentity(),
                         [] { return ::media::Status::success(); }));
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == video2);
    EXPECT_FALSE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, audioOutput->tryPop(observed));
    EXPECT_FALSE(ctx, audioOutput->tryPop(observed));
    EXPECT_TRUE(ctx, boundInput->push(transactionResult.value()));
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_FALSE(ctx, videoOutput->tryPop(observed));
    EXPECT_FALSE(ctx, audioOutput->tryPop(observed));

    auto activeRelease = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("task4-group"),
        MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch(), origin(), {{video0, 0}}, {{audio0, 0}});
    EXPECT_TRUE(ctx, activeRelease);
    if (!activeRelease) return;
    auto activeTransaction = MediaStartupReleaseTransactionBuffer::create(
        activeRelease.value());
    EXPECT_TRUE(ctx, activeTransaction);
    if (!activeTransaction) return;
    EXPECT_TRUE(ctx, boundInput->push(activeTransaction.value()));
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == video0);
    EXPECT_FALSE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, audioOutput->tryPop(observed));
    EXPECT_FALSE(ctx, audioOutput->tryPop(observed));

    const auto flush = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    EXPECT_TRUE(ctx, boundInput->push(flush));
    EXPECT_TRUE(ctx, extractor.process(execution));
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == flush);
    EXPECT_TRUE(ctx, audioOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == flush);

    const auto eof = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    EXPECT_TRUE(ctx, boundInput->push(eof));
    const auto finished = extractor.process(execution);
    EXPECT_TRUE(ctx, finished &&
                         finished.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_TRUE(ctx, videoOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == eof);
    EXPECT_TRUE(ctx, audioOutput->tryPop(observed));
    EXPECT_TRUE(ctx, observed == eof);
    EXPECT_TRUE(ctx, extractor.stop(execution));
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
    MediaChannel* preparation = nullptr;
    std::shared_ptr<MediaAvStartupVideoPreparationState> preparationState;
};

std::unique_ptr<RuntimeFixture> startFixture(
    TestContext& ctx,
    bool threadedLifecycleTarget = false,
    bool preactivation = false)
{
    auto fixture = std::make_unique<RuntimeFixture>();
    fixture->model = binderFixture(threadedLifecycleTarget, preactivation);
    fixture->preparationState = fixture->model.preparationState;
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
        fixture->model.firstActivatedSink,
        fixture->model.firstActivatedPort);
    fixture->secondEvent = fixture->runtime.context().findInputChannel(
        fixture->model.secondActivatedSink, "in");
    fixture->boundRelease = fixture->runtime.context().findInputChannel(
        fixture->model.releaseSink, "in");
    if (fixture->model.preparationSink.isValid()) {
        fixture->preparation = fixture->runtime.context().findInputChannel(
            fixture->model.preparationSink, "in");
    }
    EXPECT_TRUE(ctx, fixture->binder && fixture->sequencer && fixture->group &&
                         fixture->releaseInput && fixture->transaction &&
                         fixture->firstEvent && fixture->secondEvent &&
                         fixture->boundRelease);
    return fixture;
}

void testInitialActivationWaitsForVideoFilterPreparation(TestContext& ctx)
{
    auto fixture = startFixture(ctx, false, true);
    EXPECT_TRUE(ctx, fixture->preparationState && fixture->preparation);
    if (!fixture->preparationState || !fixture->preparation ||
        !fixture->binder || !fixture->sequencer) return;
    auto initial = release(ctx);
    EXPECT_TRUE(ctx, fixture->releaseInput->push(initial));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    auto waiting = fixture->sequencer->process(fixture->runtime.context());
    EXPECT_TRUE(ctx, waiting &&
                         waiting.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    MediaBufferRef preparation;
    EXPECT_TRUE(ctx, fixture->preparation->tryPop(preparation));
    const auto* transaction = dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
        preparation.get());
    EXPECT_TRUE(ctx, transaction != nullptr);
    if (!transaction) return;
    EXPECT_TRUE(ctx, fixture->preparationState->begin(
                         epoch().generation, transaction->releaseIdentity(), 1));
    const std::span<const MediaBufferRef> noOutputs;
    const std::array<MediaAtomicOutputBatch, 1> reservationBatches{
        MediaAtomicOutputBatch{fixture->preparation, noOutputs}};
    auto filterReservation = MediaReservedOutputTransaction::reserve(
        "Sequencer filter readiness", reservationBatches);
    auto extractorReservation = MediaReservedOutputTransaction::reserve(
        "Sequencer extractor readiness", reservationBatches);
    EXPECT_TRUE(ctx, filterReservation && filterReservation.value() &&
                         extractorReservation && extractorReservation.value());
    if (!filterReservation || !filterReservation.value() ||
        !extractorReservation || !extractorReservation.value()) return;
    EXPECT_TRUE(ctx, fixture->preparationState->markFilterReady(
                         epoch().generation, transaction->releaseIdentity(),
                         filterReservation.value()->handle()));
    EXPECT_TRUE(ctx, fixture->preparationState->registerExtractorOutputs(
                         epoch().generation, transaction->releaseIdentity(),
                         extractorReservation.value()->handle()));
    const auto anchored = fixture->sequencer->process(
        fixture->runtime.context());
    EXPECT_TRUE(ctx, anchored &&
                         anchored.value().state == MediaNodeProcessState::Waiting);
    const auto anchor = fixture->preparationState->snapshot();
    EXPECT_TRUE(ctx, anchor.anchoredEpoch.has_value());
    EXPECT_TRUE(ctx, anchor.anchoredAudioOrigin.has_value());
    if (!anchor.anchoredEpoch || !anchor.anchoredAudioOrigin) return;
    EXPECT_TRUE(ctx, anchor.anchoredEpoch->masterRelease > epoch().masterRelease);
    EXPECT_EQ(ctx, anchor.anchoredAudioOrigin->masterRelease,
              anchor.anchoredEpoch->masterRelease);
    EXPECT_TRUE(ctx, fixture->preparationState->acknowledgeExtractorReanchor(
                         epoch().generation, transaction->releaseIdentity()));
    EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Active);
    EXPECT_EQ(ctx, fixture->preparationState->snapshot().phase,
              MediaAvStartupVideoPreparationPhase::ReleaseCommitted);
}

void testOpenEmptyRequiredInputsRemainWaiting(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer) return;
    const auto binder = fixture->binder->process(fixture->runtime.context());
    const auto sequencer = fixture->sequencer->process(
        fixture->runtime.context());
    EXPECT_TRUE(ctx, binder &&
                         binder.value().state == MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, sequencer &&
                         sequencer.value().state == MediaNodeProcessState::Waiting);
}

void testRequiredInputTerminalStateFailsPermanently(TestContext& ctx)
{
    const auto verify = [&](bool binderInput, bool abort) {
        auto fixture = startFixture(ctx);
        auto* channel = binderInput ? fixture->releaseInput
                                    : fixture->transaction;
        if (!fixture->binder || !fixture->sequencer || !channel) return;
        if (abort) channel->abort();
        else channel->close();
        auto first = binderInput
            ? fixture->binder->process(fixture->runtime.context())
            : fixture->sequencer->process(fixture->runtime.context());
        EXPECT_FALSE(ctx, first);
        if (first) return;
        const std::string expected = std::string(
            binderInput ? "Playback epoch binder" :
                          "Activation release sequencer") +
            (abort
                 ? " required input aborted before a buffer was available: "
                 : " required input closed before a buffer was available: ") +
            (binderInput ? "release" : "transaction");
        EXPECT_EQ(ctx, first.error().code, ::media::ErrorCode::Cancelled);
        EXPECT_EQ(ctx, first.error().message, expected);
        auto repeated = binderInput
            ? fixture->binder->process(fixture->runtime.context())
            : fixture->sequencer->process(fixture->runtime.context());
        EXPECT_FALSE(ctx, repeated);
        if (!repeated) {
            EXPECT_EQ(ctx, repeated.error().code, first.error().code);
            EXPECT_EQ(ctx, repeated.error().message, first.error().message);
        }
    };
    verify(true, false);
    verify(true, true);
    verify(false, false);
    verify(false, true);
}

void testEofTraversesBinderAndSequencerWithoutEpochActivation(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer || !fixture->group) return;
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, fixture->releaseInput->push(eof.value()));
    const auto bound = fixture->binder->process(fixture->runtime.context());
    EXPECT_TRUE(ctx, bound &&
                         bound.value().state == MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, fixture->transaction->size(), static_cast<std::size_t>(1));
    const auto sequenced = fixture->sequencer->process(
        fixture->runtime.context());
    EXPECT_TRUE(ctx, sequenced &&
                         sequenced.value().state == MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, fixture->group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
    EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
    MediaBufferRef output;
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(output));
    EXPECT_TRUE(ctx, output == eof.value());
}

void testEofReleaseIsAtomicAcrossBackpressure(TestContext& ctx)
{
    auto fixture = startFixture(ctx);
    if (!fixture->binder || !fixture->sequencer) return;
    auto filler = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof && fixture->boundRelease->push(filler));
    if (!eof) return;
    EXPECT_TRUE(ctx, fixture->releaseInput->push(eof.value()));
    EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
    const auto blocked = fixture->sequencer->process(fixture->runtime.context());
    EXPECT_TRUE(ctx, blocked &&
                         blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
    MediaBufferRef discarded;
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(discarded));
    const auto committed = fixture->sequencer->process(
        fixture->runtime.context());
    EXPECT_TRUE(ctx, committed &&
                         committed.value().state == MediaNodeProcessState::Finished);
    MediaBufferRef output;
    EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(output));
    EXPECT_TRUE(ctx, output == eof.value());
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

    for (std::int64_t time = 2; time <= 3; ++time) {
        const auto payload = makeMediaBufferRef<MediaAvStartupClockBuffer>(
            ms(time));
        auto passThrough = MediaAvStartupReleaseBuffer::create(
            MediaAvSyncGroupKey("task4-group"),
            MediaAvStartupReleaseKind::ActiveEpochPassThrough,
            epoch(), origin(), {{payload, 0}}, {});
        EXPECT_TRUE(ctx, passThrough);
        if (!passThrough) return;
        EXPECT_TRUE(ctx, fixture->releaseInput->push(passThrough.value()));
        EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
        EXPECT_TRUE(ctx, fixture->sequencer->process(fixture->runtime.context()));
        MediaBufferRef forwarded;
        EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(forwarded));
        EXPECT_TRUE(ctx, forwarded == passThrough.value());
        EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->firstEvent->metrics().pushed,
                  static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, fixture->secondEvent->metrics().pushed,
                  static_cast<std::uint64_t>(1));
    }
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

void testThreadedStopAndAbortDiscardRetainedReleaseTransaction(
    TestContext& ctx)
{
    const auto run = [&](bool abort) {
        auto fixture = startFixture(ctx, true);
        if (!fixture->binder || !fixture->sequencer ||
            !fixture->releaseInput || !fixture->firstEvent) return;
        auto filler = makeMediaBufferRef<MediaAvStartupClockBuffer>(ms(0));
        EXPECT_TRUE(ctx, fixture->firstEvent->push(filler));
        filler.reset();
        auto retained = release(ctx);
        MediaBufferWeakRef retainedWeak = retained;
        EXPECT_TRUE(ctx, fixture->releaseInput->push(retained));
        retained.reset();
        EXPECT_TRUE(ctx, fixture->binder->process(fixture->runtime.context()));
        auto waiting = fixture->sequencer->process(
            fixture->runtime.context());
        EXPECT_TRUE(ctx, waiting &&
                             waiting.value().state ==
                                 MediaNodeProcessState::Waiting);
        EXPECT_FALSE(ctx, retainedWeak.expired());
        EXPECT_EQ(ctx, fixture->group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
        EXPECT_TRUE(ctx, fixture->runtime.startThreaded());
        if (abort) fixture->runtime.abort();
        else EXPECT_TRUE(ctx, fixture->runtime.stop());
        EXPECT_EQ(ctx, fixture->runtime.state(),
                  abort ? MediaGraphRuntimeState::Aborted
                        : MediaGraphRuntimeState::Stopped);
        EXPECT_EQ(ctx,
                  fixture->runtime.threadedExecutor().metrics().activeWorkers,
                  static_cast<std::size_t>(0));
        EXPECT_TRUE(ctx, retainedWeak.expired());
        EXPECT_EQ(ctx, fixture->releaseInput->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->transaction->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->boundRelease->size(), static_cast<std::size_t>(0));
        EXPECT_FALSE(ctx, fixture->runtime.startThreaded());
    };
    run(false);
    run(true);
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

    MediaRtpLockedClockGroup repeatedLocked{
        ms(10), {'c'}, calibration, calibration};
    auto repeatedSnapshot = makeMediaBufferRef<MediaRtpClockGroupBuffer>(
        MediaRtpClockGroupSnapshot{
            MediaRtpClockGroupState::Locked, 1,
            std::move(repeatedLocked)});
    EXPECT_TRUE(ctx, execution.findInputChannel(adapter, "clock")->push(
                         repeatedSnapshot));
    const auto repeated = node.process(execution);
    EXPECT_TRUE(ctx, repeated &&
                         repeated.value().state == MediaNodeProcessState::Progress);
    EXPECT_EQ(ctx, execution.findInputChannel(sink, "state")->size(),
              std::size_t{0});

    auto degradedSnapshot = makeMediaBufferRef<MediaRtpClockGroupBuffer>(
        MediaRtpClockGroupSnapshot{
            MediaRtpClockGroupState::Degraded, 1, std::nullopt});
    EXPECT_TRUE(ctx, execution.findInputChannel(adapter, "clock")->push(
                         degradedSnapshot));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(sink, "state")->tryPop(output));
    state = dynamic_cast<const MediaSourceClockStateBuffer*>(output.get());
    EXPECT_TRUE(ctx, state &&
                         state->readiness() == MediaSourceClockReadiness::Degraded &&
                         state->generation() == 1);
}

void testControlTransactionPreservesExactReference(TestContext& ctx)
{
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    auto envelope = MediaStartupReleaseTransactionBuffer::createControl(
        eof.value());
    EXPECT_TRUE(ctx, envelope);
    const auto* transaction = envelope
        ? dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
              envelope.value().get())
        : nullptr;
    EXPECT_TRUE(ctx, transaction != nullptr);
    if (transaction) {
        EXPECT_EQ(ctx, transaction->transactionKind(),
                  MediaStartupReleaseTransactionKind::Control);
        EXPECT_TRUE(ctx, transaction->payload() == eof.value());
    }
    EXPECT_FALSE(ctx, MediaStartupReleaseTransactionBuffer::createControl(
                          makeMediaBufferRef<MediaControlBuffer>(
                              MediaControlBufferKind::Unknown)));
}

void testRtpAdapterPropagatesTerminalWithoutInventingClockState(TestContext& ctx)
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
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    EXPECT_TRUE(ctx, execution.findInputChannel(adapter, "clock")->push(eof.value()));
    const auto result = node.process(execution);
    EXPECT_TRUE(ctx, result &&
                         result.value().state == MediaNodeProcessState::Finished);
    MediaBufferRef output;
    EXPECT_TRUE(ctx, execution.findInputChannel(sink, "state")->tryPop(output));
    EXPECT_TRUE(ctx, output == eof.value() && output->isEof());
    EXPECT_TRUE(ctx, execution.findInputChannel(sink, "state")->size() == 0);
}

void testTypedControlsTraverseReleaseWithoutActivation(TestContext& ctx)
{
    for (const auto kind : {MediaControlBufferKind::Flush,
                            MediaControlBufferKind::Abort}) {
        auto fixture = startFixture(ctx);
        if (!fixture->binder || !fixture->sequencer || !fixture->group) return;
        auto control = makeMediaBufferRef<MediaControlBuffer>(kind);
        EXPECT_TRUE(ctx, fixture->releaseInput->push(control));
        const auto expected = kind == MediaControlBufferKind::Flush
            ? MediaNodeProcessState::Progress
            : MediaNodeProcessState::Finished;
        const auto bound = fixture->binder->process(fixture->runtime.context());
        const auto sequenced = fixture->sequencer->process(
            fixture->runtime.context());
        EXPECT_TRUE(ctx, bound && bound.value().state == expected);
        EXPECT_TRUE(ctx, sequenced && sequenced.value().state == expected);
        EXPECT_EQ(ctx, fixture->group->lifecycleState(),
                  MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch);
        EXPECT_EQ(ctx, fixture->firstEvent->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, fixture->secondEvent->size(), static_cast<std::size_t>(0));
        MediaBufferRef output;
        EXPECT_TRUE(ctx, fixture->boundRelease->tryPop(output));
        EXPECT_TRUE(ctx, output == control);
    }
}

void testRtpAdapterPropagatesTypedControlsAndFailsClosed(TestContext& ctx)
{
    const auto verify = [&](std::optional<MediaControlBufferKind> kind,
                            bool abortInput) {
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
        MediaBufferRef control;
        if (kind) {
            control = makeMediaBufferRef<MediaControlBuffer>(*kind);
            EXPECT_TRUE(ctx, execution.findInputChannel(adapter, "clock")->push(
                                 control));
        } else if (abortInput) {
            execution.findInputChannel(adapter, "clock")->abort();
        } else {
            execution.findInputChannel(adapter, "clock")->close();
        }
        const auto result = node.process(execution);
        if (!kind) {
            EXPECT_FALSE(ctx, result);
            if (!result) EXPECT_EQ(ctx, result.error().code, ::media::ErrorCode::Cancelled);
            const auto repeated = node.process(execution);
            EXPECT_FALSE(ctx, repeated);
            if (!result && !repeated) {
                EXPECT_EQ(ctx, repeated.error().code, result.error().code);
                EXPECT_EQ(ctx, repeated.error().message, result.error().message);
            }
            return;
        }
        const auto expected = *kind == MediaControlBufferKind::Flush
            ? MediaNodeProcessState::Progress
            : MediaNodeProcessState::Finished;
        EXPECT_TRUE(ctx, result && result.value().state == expected);
        MediaBufferRef output;
        EXPECT_TRUE(ctx, execution.findInputChannel(sink, "state")->tryPop(output));
        EXPECT_TRUE(ctx, output == control);
    };
    verify(MediaControlBufferKind::Flush, false);
    verify(MediaControlBufferKind::Abort, false);
    verify(std::nullopt, false);
    verify(std::nullopt, true);
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
    MediaBufferRef second;
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(second));
    auto flush = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(clockNode, "clock")->push(
                         flush));
    const auto flushed = node->process(runtime.context());
    EXPECT_TRUE(ctx, flushed && flushed.value().state ==
                                    MediaNodeProcessState::Progress);
    MediaBufferRef forwardedFlush;
    EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(
                         forwardedFlush));
    EXPECT_TRUE(ctx, forwardedFlush == flush);
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    if (eof) {
        EXPECT_TRUE(ctx, runtime.context().findInputChannel(clockNode, "clock")->push(
                             eof.value()));
        const auto terminal = node->process(runtime.context());
        EXPECT_TRUE(ctx, terminal && terminal.value().state ==
                                         MediaNodeProcessState::Finished);
        MediaBufferRef forwarded;
        EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(
                             forwarded));
        EXPECT_TRUE(ctx, forwarded == eof.value());
    }
}

void testStartupClockFailsClosedBeforeLockAndOnMissingTerminal(TestContext& ctx)
{
    const auto verify = [&](int termination) {
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
        MediaGraphRuntime runtime;
        EXPECT_TRUE(ctx, runtime.compile(std::move(fixture.executable)));
        EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
        auto* node = dynamic_cast<MediaAvStartupClockNode*>(
            runtime.scheduler().findNode(clockNode));
        EXPECT_TRUE(ctx, node != nullptr);
        if (!node) return;
        EXPECT_TRUE(ctx, node->start(runtime.context()));
        MediaChannel* input = runtime.context().findInputChannel(clockNode, "clock");
        if (termination == 3) {
            EXPECT_TRUE(ctx, input->push(
                                 makeMediaBufferRef<MediaSourceClockStateBuffer>(
                                     MediaSourceClockReadiness::Locked, 1, false)));
            EXPECT_TRUE(ctx, node->process(runtime.context()));
            MediaBufferRef tick;
            EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(
                                 tick));
            auto abort = makeMediaBufferRef<MediaControlBuffer>(
                MediaControlBufferKind::Abort);
            EXPECT_TRUE(ctx, input->push(abort));
            const auto finished = node->process(runtime.context());
            EXPECT_TRUE(ctx, finished && finished.value().state ==
                                              MediaNodeProcessState::Finished);
            MediaBufferRef forwarded;
            EXPECT_TRUE(ctx, runtime.context().findInputChannel(sink, "tick")->tryPop(
                                 forwarded));
            EXPECT_TRUE(ctx, forwarded == abort);
            return;
        }
        if (termination == 0) {
            auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
            EXPECT_TRUE(ctx, eof && input->push(eof.value()));
        } else if (termination == 1) {
            input->close();
        } else {
            input->abort();
        }
        const auto failed = node->process(runtime.context());
        EXPECT_FALSE(ctx, failed);
        if (!failed) EXPECT_EQ(ctx, failed.error().code, ::media::ErrorCode::Cancelled);
        EXPECT_EQ(ctx, runtime.context().findInputChannel(sink, "tick")->size(),
                  static_cast<std::size_t>(0));
    };
    verify(0);
    verify(1);
    verify(2);
    verify(3);
}

void testBoundReleaseAtomicOutputPolicyMigrationIsExplicit(TestContext& ctx)
{
    const auto verify = [&](MediaEdgePolicy outputPolicy, bool accepted) {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto extractor = graph.addNode(
            MediaNodeKind::AvBoundReleaseExtractor, "extractor");
        const auto video = graph.addNode(MediaNodeKind::DebugDump, "video");
        const auto audio = graph.addNode(MediaNodeKind::DebugDump, "audio");
        graph.addOutputPort(source, "release", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(extractor, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(extractor, "video", MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(extractor, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(video, "in", MediaStreamKind::Video,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(audio, "in", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        EXPECT_TRUE(ctx, graph.connect(
                             source, "release", extractor, "in", "release",
                             MediaGraphBuildSupport::blockingQueuePolicy(1)));
        EXPECT_TRUE(ctx, graph.connect(
                             extractor, "video", video, "in", "video",
                             outputPolicy));
        EXPECT_TRUE(ctx, graph.connect(
                             extractor, "audio", audio, "in", "audio",
                             outputPolicy));
        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));
        MediaAvBoundReleaseExtractorNode node(extractor);
        const auto started = node.start(execution);
        if (accepted) {
            EXPECT_TRUE(ctx, started);
            auto eof = makeMediaBufferRef<MediaControlBuffer>(
                MediaControlBufferKind::Eof);
            EXPECT_TRUE(ctx, execution.findInputChannel(extractor, "in")->push(eof));
            const auto result = node.process(execution);
            EXPECT_TRUE(ctx, result && result.value().state ==
                                          MediaNodeProcessState::Finished);
            EXPECT_EQ(ctx, execution.findInputChannel(video, "in")->size(),
                      static_cast<std::size_t>(1));
            EXPECT_EQ(ctx, execution.findInputChannel(audio, "in")->size(),
                      static_cast<std::size_t>(1));
        } else {
            EXPECT_FALSE(ctx, started);
            if (!started) {
                EXPECT_EQ(ctx, started.error().code,
                          ::media::ErrorCode::InvalidArgument);
            }
            EXPECT_EQ(ctx, execution.findInputChannel(extractor, "in")->size(),
                      static_cast<std::size_t>(0));
            EXPECT_EQ(ctx, execution.findInputChannel(video, "in")->size(),
                      static_cast<std::size_t>(0));
            EXPECT_EQ(ctx, execution.findInputChannel(audio, "in")->size(),
                      static_cast<std::size_t>(0));
        }
    };

    const auto exact = MediaGraphBuildSupport::blockingQueuePolicy(1);
    verify(exact, true);
    auto dropping = exact;
    dropping.queuePolicy.overflowPolicy =
        MediaQueueOverflowPolicy::DropNewest;
    verify(dropping, false);
    auto reordered = exact;
    reordered.queuePolicy.orderingPolicy =
        MediaQueueOrderingPolicy::Timestamp;
    verify(reordered, false);
    auto notPreserved = exact;
    notPreserved.queuePolicy.preserveOrdering = false;
    verify(notPreserved, false);
}

} // namespace

int main()
{
    TestContext ctx;
    testTaskFourRuntimeKindsAreAppendOnly(ctx);
    testActivatedEventIsCompleteAndImmutable(ctx);
    testActivationReleaseTransactionPreservesReferences(ctx);
    testPreparationPrefixIsNotReleasedTwiceAfterActivation(ctx);
    testControlTransactionPreservesExactReference(ctx);
    testOpenEmptyRequiredInputsRemainWaiting(ctx);
    testInitialActivationWaitsForVideoFilterPreparation(ctx);
    testRequiredInputTerminalStateFailsPermanently(ctx);
    testEofTraversesBinderAndSequencerWithoutEpochActivation(ctx);
    testEofReleaseIsAtomicAcrossBackpressure(ctx);
    testTypedControlsTraverseReleaseWithoutActivation(ctx);
    testBinderWaitsForTransactionCapacityBeforeActivation(ctx);
    testSequencerCommitsOneEventToAllTargetsBeforeRelease(ctx);
    testEveryBlockedSequencerTargetPreventsPrefixVisibility(ctx);
    testActivePassThroughDoesNotReactivate(ctx);
    testClosedSequencerTargetFailsBeforeActivation(ctx);
    testGenerationMismatchAndAbortedTargetFailClosed(ctx);
    testThreadedStopAndAbortDiscardRetainedReleaseTransaction(ctx);
    testTaskFourFactorySurfaceIsComplete(ctx);
    testRtpAdapterPublishesGenericLockedState(ctx);
    testRtpAdapterPropagatesTerminalWithoutInventingClockState(ctx);
    testRtpAdapterPropagatesTypedControlsAndFailsClosed(ctx);
    testStartupClockUsesRegisteredMasterDeadline(ctx);
    testStartupClockFailsClosedBeforeLockAndOnMissingTerminal(ctx);
    testBoundReleaseAtomicOutputPolicyMigrationIsExplicit(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
