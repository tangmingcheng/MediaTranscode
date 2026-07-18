#include "unit/fixtures/ScheduledMpegTsOutputIntegrationRuntime.h"

#include "common/AvSyncRuntimeTestSupport.h"
#include "unit/fixtures/ScheduledMpegTsDecodeSampleFixture.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/output/FileOutputNode.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"
#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"
#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace media_transcode::test {
namespace {

using namespace ::media::ffmpeg::graph;

constexpr MediaRunningTime milliseconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class RealtimeIntegrationClock final : public MediaMasterClock {
public:
    RealtimeIntegrationClock() : m_started(std::chrono::steady_clock::now()) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - m_started).count()));
    }

private:
    std::chrono::steady_clock::time_point m_started;
};

struct IntegrationGraph final {
    MediaGraph graph;
    MediaEndpoint canonicalVideo;
    MediaEndpoint canonicalAudio;
    MediaEndpoint videoCodec;
    MediaEndpoint audioCodec;
    MediaNodeId binder;
    MediaNodeId sequencer;
    MediaNodeId scheduler;
    MediaNodeId router;
    MediaNodeId planSource;
    MediaNodeId adapter;
    MediaNodeId fileOutput;
    MediaNodeId mux;
};

::media::ErrorInfo integrationError(std::string message)
{
    return ::media::ErrorInfo::internalError(std::move(message));
}

MediaEndpoint addSource(MediaGraph& graph,
                        const char* name,
                        const char* port,
                        MediaStreamKind stream,
                        MediaEdgeKind edge,
                        MediaPayloadKind payload)
{
    const MediaNodeId node = graph.addNode(MediaNodeKind::DebugDump, name);
    graph.addOutputPort(node, port, stream, edge, payload);
    return {node, port};
}

::media::Result<IntegrationGraph> buildGraph(
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    IntegrationGraph result;
    result.canonicalVideo = addSource(
        result.graph, "scheduled-ts.canonical.video", "canonical",
        MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);
    result.canonicalAudio = addSource(
        result.graph, "scheduled-ts.canonical.audio", "canonical",
        MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);
    result.videoCodec = addSource(
        result.graph, "scheduled-ts.codec.video", "codec",
        MediaStreamKind::Video, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);
    result.audioCodec = addSource(
        result.graph, "scheduled-ts.codec.audio", "codec",
        MediaStreamKind::Audio, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);

    result.binder = result.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "scheduled-ts.epoch.binder");
    result.sequencer = result.graph.addNode(
        MediaNodeKind::ActivatedStartupReleaseSequencer,
        "scheduled-ts.epoch.sequencer");
    const MediaNodeId releaseSource = result.graph.addNode(
        MediaNodeKind::DebugDump, "scheduled-ts.epoch.release");
    if (!result.graph.setNodeOption(
            result.binder, "playback_epoch_binder.sync_group",
            plan.groupKey.value()) ||
        !result.graph.setNodeOption(
            result.sequencer,
            "activated_startup_release_sequencer.sync_group",
            plan.groupKey.value())) {
        return ::media::Result<IntegrationGraph>::failure(
            integrationError("scheduled TS graph could not configure epoch authority"));
    }
    result.graph.addOutputPort(
        releaseSource, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    result.graph.addInputPort(
        result.binder, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    result.graph.addOutputPort(
        result.binder, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    result.graph.addInputPort(
        result.sequencer, "transaction", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    result.graph.addOutputPort(
        result.sequencer, "activated", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    result.graph.addOutputPort(
        result.sequencer, "bound_release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const MediaNodeId releaseSink = result.graph.addNode(
        MediaNodeKind::DebugDump, "scheduled-ts.epoch.bound-release");
    result.graph.addInputPort(
        releaseSink, "release", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto eventPolicy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    if (!result.graph.connect(releaseSource, "release", result.binder,
                              "release", "release -> binder", eventPolicy) ||
        !result.graph.connect(result.binder, "transaction", result.sequencer,
                              "transaction", "binder -> sequencer", eventPolicy) ||
        !result.graph.connect(result.sequencer, "bound_release", releaseSink,
                              "release", "sequencer -> release sink", eventPolicy)) {
        return ::media::Result<IntegrationGraph>::failure(
            integrationError("scheduled TS graph could not connect epoch authority"));
    }

    auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(
        result.graph,
        {"scheduled-ts", result.canonicalVideo, result.canonicalAudio}, plan);
    if (!scheduled || !scheduled.value().serialized.valid()) {
        return ::media::Result<IntegrationGraph>::failure(
            scheduled ? integrationError("scheduled TS graph has no serialized router")
                      : scheduled.error());
    }
    result.router = scheduled.value().serialized.node;
    for (const MediaNode& node : result.graph.nodes()) {
        if (node.kind == MediaNodeKind::AvOutputScheduler) {
            if (result.scheduler.isValid()) {
                return ::media::Result<IntegrationGraph>::failure(
                    integrationError("scheduled TS graph has duplicate schedulers"));
            }
            result.scheduler = node.id;
        }
    }
    auto output = MediaScheduledMpegTsOutputSegmentBuilder::build(
        result.graph,
        {"scheduled-ts.output", {result.sequencer, "activated"},
         result.videoCodec, result.audioCodec, scheduled.value().serialized},
        plan);
    if (!output) {
        return ::media::Result<IntegrationGraph>::failure(output.error());
    }
    result.planSource = output.value().planSource;
    result.adapter = output.value().adapter;
    result.fileOutput = output.value().fileOutput;
    result.mux = output.value().mux;
    if (!result.scheduler.isValid()) {
        return ::media::Result<IntegrationGraph>::failure(
            integrationError("scheduled TS graph did not create a scheduler"));
    }
    return ::media::Result<IntegrationGraph>::success(std::move(result));
}

::media::Result<MediaBufferRef> canonicalAccessUnit(
    const ScheduledMpegTsDecodeAccessUnit& unit,
    const MediaPlaybackEpoch& epoch,
    std::uint64_t sequence)
{
    auto packet = FFmpegBufferFactory::clonePacket(
        unit.packet.get(), unit.stream == MediaScheduledStream::Video
            ? MediaStreamKind::Video : MediaStreamKind::Audio);
    if (!packet) return ::media::Result<MediaBufferRef>::failure(packet.error());
    auto presentation = epoch.sourceStart.checkedAdd(unit.presentationOnMaster);
    auto dispatch = epoch.sourceStart.checkedAdd(unit.dispatchOffset);
    if (!presentation || !dispatch) {
        return ::media::Result<MediaBufferRef>::failure(
            !presentation ? presentation.error() : dispatch.error());
    }
    auto lineage = createMediaCanonicalLineage(
        presentation.value(), dispatch.value(), milliseconds(1),
        unit.stream == MediaScheduledStream::Video
            ? MediaDecodeOrderMode::ReorderedRequiresDecodeTime
            : MediaDecodeOrderMode::PresentationOrderNoReorder,
        unit.stream == MediaScheduledStream::Video
            ? "scheduled-ts.video" : "scheduled-ts.audio",
        MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked, epoch.generation);
    if (!lineage) {
        return ::media::Result<MediaBufferRef>::failure(lineage.error());
    }
    return MediaCanonicalAccessUnitBuffer::create(
        std::move(packet).value(), std::move(lineage).value());
}

::media::Status waitForDeadline(
    const MediaMasterClock& clock,
    const MediaNodeProcessResult::DeadlineWait& wait)
{
    auto now = clock.now();
    if (!now) return ::media::Status::failure(now.error());
    if (wait.masterDeadline <= now.value()) return ::media::Status::success();
    auto remaining = wait.masterDeadline.checkedSubtract(now.value());
    if (!remaining) return ::media::Status::failure(remaining.error());
    constexpr auto maximumDiagnosticWait = std::chrono::seconds(5);
    if (remaining.value().nanoseconds() > maximumDiagnosticWait.count() *
            1'000'000'000LL) {
        return ::media::Status::failure(integrationError(
            "scheduled TS runtime requested a deadline wait of " +
            std::to_string(remaining.value().nanoseconds()) +
            " ns at master time " +
            std::to_string(now.value().nanoseconds()) + " ns"));
    }
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(remaining.value().nanoseconds()));
    return ::media::Status::success();
}

template <typename Node>
Node* runtimeNode(MediaGraphRuntime& runtime, MediaNodeId id)
{
    return dynamic_cast<Node*>(runtime.scheduler().findNode(id));
}

} // namespace

::media::Status ScheduledMpegTsOutputIntegrationRuntime::write(
    const MediaRealtimeAvSyncRuntimePlan& plan,
    const MediaPlaybackEpoch& epoch,
    ScheduledMpegTsDecodeSampleFixture& sample)
{
    auto built = buildGraph(plan);
    if (!built) return ::media::Status::failure(built.error());
    auto clock = std::make_shared<RealtimeIntegrationClock>();
    MediaGraphRuntime runtime(
        std::make_shared<FixedAvSyncClockSource>(clock));
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(built.value().graph);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        plan.groupKey, plan.synchronization, plan.transition});
    if (auto compiled = runtime.compile(std::move(executable)); !compiled) {
        return ::media::Status::failure(compiled.error());
    }
    std::cerr << "[scheduled-ts-runtime] compile complete\n";
    if (auto registered = runtime.registerDefaultRuntimeNodes(); !registered) {
        return ::media::Status::failure(registered.error());
    }
    std::cerr << "[scheduled-ts-runtime] registration complete\n";

    auto* scheduler = runtimeNode<MediaAvOutputSchedulerNode>(
        runtime, built.value().scheduler);
    auto* router = runtimeNode<MediaScheduledOutputRouterNode>(
        runtime, built.value().router);
    auto* planSource = runtimeNode<MediaProjectMpegTsPlanSourceNode>(
        runtime, built.value().planSource);
    auto* adapter = runtimeNode<MediaScheduledTsAccessUnitAdapterNode>(
        runtime, built.value().adapter);
    auto* fileOutput = runtimeNode<FileOutputNode>(
        runtime, built.value().fileOutput);
    auto* mux = runtimeNode<FileMuxNode>(runtime, built.value().mux);
    if (!scheduler || !router || !planSource || !adapter || !fileOutput || !mux) {
        return ::media::Status::failure(
            integrationError("scheduled TS factory did not create production nodes"));
    }
    auto& context = runtime.context();
    for (auto* node : std::array<FFmpegNodeRuntime*, 6>{
             scheduler, router, planSource, adapter, fileOutput, mux}) {
        if (auto started = node->start(context); !started) return started;
    }
    std::cerr << "[scheduled-ts-runtime] selected nodes started\n";
    if (!activateInitialThroughRelease(
            runtime, built.value().binder, plan.groupKey, epoch,
            sample.audioCodecContext().sample_rate)) {
        return ::media::Status::failure(
            integrationError("scheduled TS could not activate binder and sequencer"));
    }
    std::cerr << "[scheduled-ts-runtime] epoch activated\n";
    auto videoCodec = FFmpegBufferFactory::borrowCodecContext(
        &sample.videoCodecContext());
    auto audioCodec = FFmpegBufferFactory::borrowCodecContext(
        &sample.audioCodecContext());
    MediaChannel* videoCodecOutput = context.findOutputChannel(
        built.value().videoCodec.node, built.value().videoCodec.port);
    MediaChannel* audioCodecOutput = context.findOutputChannel(
        built.value().audioCodec.node, built.value().audioCodec.port);
    if (!videoCodec || !audioCodec || !videoCodecOutput || !audioCodecOutput ||
        !videoCodecOutput->push(std::move(videoCodec).value()) ||
        !audioCodecOutput->push(std::move(audioCodec).value())) {
        return ::media::Status::failure(
            integrationError("scheduled TS could not queue codec contexts"));
    }
    auto resource = fileOutput->process(context);
    auto planned = planSource->process(context);
    auto adaptedPlan = adapter->process(context);
    if (!resource || !planned || !adaptedPlan) {
        return ::media::Status::failure(
            !resource ? resource.error()
            : !planned ? planned.error() : adaptedPlan.error());
    }
    std::cerr << "[scheduled-ts-runtime] output plan and resource queued\n";
    for (std::size_t step = 0; step < 8; ++step) {
        auto binding = mux->process(context);
        if (!binding) return ::media::Status::failure(binding.error());
    }
    std::cerr << "[scheduled-ts-runtime] mux binding drive complete\n";

    MediaChannel* videoInput = context.findInputChannel(
        built.value().scheduler, "video");
    MediaChannel* audioInput = context.findInputChannel(
        built.value().scheduler, "audio");
    if (!videoInput || !audioInput) {
        return ::media::Status::failure(
            integrationError("scheduled TS lost canonical scheduler inputs"));
    }
    bool muxFinished = false;
    std::size_t sourceIndex = 0;
    MediaBufferRef pendingSource;
    bool inputsClosed = false;
    constexpr std::size_t maximumSteps = 100'000;
    const auto driveStarted = std::chrono::steady_clock::now();
    constexpr auto maximumDriveDuration = std::chrono::seconds(15);
    for (std::size_t step = 0; step < maximumSteps; ++step) {
        if (std::chrono::steady_clock::now() - driveStarted >
            maximumDriveDuration) {
            return ::media::Status::failure(integrationError(
                "scheduled TS runtime exceeded 15 second drive budget at step " +
                std::to_string(step)));
        }
        bool sourceProgress = false;
        if (sourceIndex < sample.accessUnits().size()) {
            const auto& unit = sample.accessUnits()[sourceIndex];
            if (!pendingSource) {
                auto canonical = canonicalAccessUnit(
                    unit, epoch, sourceIndex + 1);
                if (!canonical) {
                    return ::media::Status::failure(canonical.error());
                }
                pendingSource = std::move(canonical).value();
            }
            MediaChannel* input = unit.stream == MediaScheduledStream::Video
                ? videoInput : audioInput;
            switch (input->pushOutcome(pendingSource)) {
            case MediaQueuePushOutcome::Accepted:
                pendingSource.reset();
                ++sourceIndex;
                sourceProgress = true;
                break;
            case MediaQueuePushOutcome::WouldBlock:
                break;
            case MediaQueuePushOutcome::Dropped:
            case MediaQueuePushOutcome::Closed:
            case MediaQueuePushOutcome::Aborted:
                return ::media::Status::failure(integrationError(
                    "scheduled TS canonical queue rejected source unit " +
                    std::to_string(sourceIndex)));
            }
        } else if (!inputsClosed) {
            videoInput->close();
            audioInput->close();
            inputsClosed = true;
            sourceProgress = true;
            std::cerr << "[scheduled-ts-runtime] canonical media queued and inputs closed\n";
        }
        std::array<::media::Result<MediaNodeProcessResult>, 4> processed{
            scheduler->process(context), router->process(context),
            adapter->process(context), mux->process(context)};
        for (const auto& result : processed) {
            if (!result) return ::media::Status::failure(result.error());
        }
        muxFinished = muxFinished ||
            processed[3].value().state == MediaNodeProcessState::Finished;
        if (muxFinished) {
            std::cerr << "[scheduled-ts-runtime] mux finished at step "
                      << step << "\n";
            return ::media::Status::success();
        }
        const bool progressed = sourceProgress || std::any_of(
            processed.begin(), processed.end(), [](const auto& result) {
                return result.value().state == MediaNodeProcessState::Progress;
            });
        if (progressed) continue;
        const MediaNodeProcessResult::DeadlineWait* earliest = nullptr;
        for (const auto& result : processed) {
            if (!result.value().deadlineWait) continue;
            if (!earliest || result.value().deadlineWait->masterDeadline <
                                 earliest->masterDeadline) {
                earliest = &*result.value().deadlineWait;
            }
        }
        if (!earliest) {
            return ::media::Status::failure(
                integrationError("scheduled TS runtime stalled without node deadline"));
        }
        if (auto waited = waitForDeadline(*clock, *earliest); !waited) {
            return waited;
        }
    }
    return ::media::Status::failure(
        integrationError("scheduled TS runtime exceeded process-step budget"));
}

} // namespace media_transcode::test
