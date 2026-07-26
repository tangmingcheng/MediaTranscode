#include "internal/graph/nodes/sync/MediaAudioDriftControllerNode.h"
#include "internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "common/AvSyncRuntimeTestSupport.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <cassert>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace media::ffmpeg::graph;

namespace {

MediaRunningTime ms(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaBufferRef packet()
{
    auto native = ::media::ffmpeg::makePacket();
    assert(native);
    assert(av_new_packet(native.get(), 8) == 0);
    native->pts = 99'999;
    native->dts = 88'888;
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(native), MediaStreamKind::Audio, std::nullopt);
    assert(wrapped);
    return wrapped.value();
}

MediaBufferRef frame(int samples)
{
    auto native = ::media::ffmpeg::makeFrame();
    assert(native);
    native->format = AV_SAMPLE_FMT_FLTP;
    native->sample_rate = 48'000;
    native->nb_samples = samples;
    av_channel_layout_default(&native->ch_layout, 2);
    assert(av_frame_get_buffer(native.get(), 0) == 0);
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(native), MediaStreamKind::Audio);
    assert(wrapped);
    return wrapped.value();
}

std::shared_ptr<const MediaCanonicalLineage> lineage(
    std::int64_t presentationMs,
    std::int64_t durationMs,
    std::uint64_t generation,
    std::uint64_t sequence)
{
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        ms(presentationMs), std::nullopt, ms(durationMs),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "audio-source",
        MediaSourceAccessUnitSequence(sequence), MediaTimeMappingConfidence::Locked,
        generation});
}

MediaBufferRef encoded(
    std::vector<MediaAudioIntervalFragment> fragments,
    MediaAudioPlaybackOrigin origin)
{
    auto value = MediaEncodedAudioLineageBuffer::create(
        packet(), std::move(fragments), origin);
    assert(value);
    return value.value();
}

void canonicalizerUsesFragmentsNotPacketPts()
{
    const MediaAudioPlaybackOrigin origin{7, ms(100), ms(500), 4'800, 48'000};
    auto input = encoded(
        {{lineage(100, 10, 7, 1), {4'800, 5'280, 48'000}},
         {lineage(110, 10, 7, 2), {5'280, 5'760, 48'000}}},
        origin);
    auto first = MediaEncodedAudioCanonicalizerNode::canonicalize(
        input, MediaSourceAccessUnitSequence(1));
    assert(first);
    assert(first.value()->stream() == MediaScheduledStream::Audio);
    assert(first.value()->generation() == 7);
    assert(first.value()->canonicalPresentation() == ms(100));
    assert(first.value()->canonicalDuration() == ms(20));
    assert(first.value()->sourceSequence() == MediaSourceAccessUnitSequence(1));
}

void canonicalizerRejectsGapOverlapAndGenerationMismatch()
{
    const MediaAudioPlaybackOrigin origin{7, ms(100), ms(500), 4'800, 48'000};
    auto gap = MediaEncodedAudioLineageBuffer::create(
        packet(),
        {{lineage(100, 10, 7, 1), {4'800, 5'280, 48'000}},
         {lineage(111, 10, 7, 2), {5'281, 5'760, 48'000}}},
        origin);
    assert(!gap);
    auto overlap = MediaEncodedAudioLineageBuffer::create(
        packet(),
        {{lineage(100, 10, 7, 1), {4'800, 5'280, 48'000}},
         {lineage(109, 10, 7, 2), {5'279, 5'760, 48'000}}},
        origin);
    assert(!overlap);
    auto wrongGeneration = MediaEncodedAudioLineageBuffer::create(
        packet(), {{lineage(100, 10, 8, 1), {4'800, 5'280, 48'000}}}, origin);
    assert(!wrongGeneration);
}

void driftMeasurementUsesActiveEpochAndCanonicalAudioEnd()
{
    const MediaAudioPlaybackOrigin origin{7, ms(100), ms(500), 4'800, 48'000};
    auto measurement = MediaAudioDriftControllerNode::measureCanonicalPosition(
        origin, ms(515), {4'800, 5'280, 48'000}, 1);
    assert(measurement);
    assert(measurement.value().generation == 7);
    assert(measurement.value().sequence == 1);
    assert(measurement.value().effectiveOutputSampleIndex == 4'800);
    assert(measurement.value().phaseError == ms(5));
    assert(measurement.value().sourceEndOnMaster == ms(515));
}

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(MediaRunningTime now) : m_now(now) {}
    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(m_now);
    }
private:
    MediaRunningTime m_now;
};

MediaAvSyncPlan completePlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "task6-audio-control";
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
    assert(planned);
    auto plan = std::move(planned).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    return plan;
}

MediaBufferRef boundAudio(std::int64_t begin,
                          std::int64_t end,
                          MediaAudioPlaybackOrigin origin,
                          std::uint64_t sourceSequence)
{
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        frame(static_cast<int>(end - begin)),
        lineage(begin / 48, (end - begin) / 48,
                origin.generation, sourceSequence),
        {begin, end, 48'000});
    assert(canonical);
    auto bound = MediaBoundCanonicalAudioBuffer::create(
        canonical.value(), origin);
    assert(bound);
    return bound.value();
}

void controllerRetainsAtomicCorrectionBeforeAudioAcrossBackpressure()
{
    MediaGraph graph;
    const auto scheduler = graph.addNode(MediaNodeKind::AvOutputScheduler, "scheduler");
    const auto binder = graph.addNode(MediaNodeKind::PlaybackEpochBinder, "binder");
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto controller = graph.addNode(
        MediaNodeKind::AudioDriftController, "audio-drift-controller");
    const auto correctionSink = graph.addNode(MediaNodeKind::DebugDump, "correction-sink");
    const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio-sink");
    graph.setNodeOption(scheduler, "av_scheduler.sync_group", "task6-group");
    graph.setNodeOption(binder, "playback_epoch_binder.sync_group", "task6-group");
    graph.setNodeOption(controller, "audio_drift_controller.sync_group", "task6-group");
    graph.addOutputPort(source, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(controller, "audio", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(controller, "correction", MediaStreamKind::Audio,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(controller, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(correctionSink, "correction", MediaStreamKind::Audio,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(audioSink, "audio", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    auto inputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(2);
    auto outputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    assert(graph.connect(source, "audio", controller, "audio", "audio", inputPolicy));
    assert(graph.connect(controller, "correction", correctionSink, "correction",
                         "correction", outputPolicy));
    assert(graph.connect(controller, "audio", audioSink, "audio", "audio", outputPolicy));

    const auto plan = completePlan();
    const auto transition = MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp, ms(1'000), ms(1'000));
    auto clock = std::make_shared<TestMasterClock>(ms(505));
    MediaGraphExecutionContext execution;
    std::unique_ptr<MediaGraphRuntime> runtime;
    const MediaPlaybackEpoch epoch{ms(100), ms(500), 1};
    assert(media_transcode::test::compileAndActivateAvSyncRuntime(
        std::move(graph),
        {MediaAvSyncGroupKey("task6-group"), plan, transition,
         MediaAvSyncBindingAssemblyMode::ComponentCore},
        clock, epoch, binder, execution, runtime));
    auto* node = dynamic_cast<MediaAudioDriftControllerNode*>(
        runtime->scheduler().findNode(controller));
    assert(node);
    assert(node->start(execution));
    const MediaAudioPlaybackOrigin origin{1, ms(100), ms(500), 0, 48'000};
    auto held = boundAudio(0, 480, origin, 1);
    assert(execution.findOutputChannel(controller, "audio")->push(held));
    assert(execution.findInputChannel(controller, "audio")->push(held));
    auto blocked = node->process(execution);
    assert(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    assert(execution.findOutputChannel(controller, "correction")->size() == 0);
    assert(execution.findOutputChannel(controller, "audio")->size() == 1);
    MediaBufferRef released;
    assert(execution.findOutputChannel(controller, "audio")->tryPop(released));
    auto committed = node->process(execution);
    assert(committed && committed.value().state == MediaNodeProcessState::Progress);
    MediaBufferRef correction;
    MediaBufferRef audio;
    assert(execution.findOutputChannel(controller, "correction")->tryPop(correction));
    const auto* typedCorrection = dynamic_cast<const MediaAudioCorrectionBuffer*>(
        correction.get());
    assert(typedCorrection);
    assert(typedCorrection->command().generation() == 1);
    assert(typedCorrection->command().sequence() == 1);
    assert(typedCorrection->command().effectiveOutputSampleIndex() == 0);
    assert(std::abs(typedCorrection->command().stretchPpm()) <=
           *plan.audioServo.recoveryCorrectionLimitPpm);
    assert(execution.findOutputChannel(controller, "audio")->tryPop(audio));
    assert(audio == held);

    auto flush = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    assert(execution.findInputChannel(controller, "audio")->push(flush));
    const auto flushed = node->process(execution);
    assert(flushed && flushed.value().state == MediaNodeProcessState::Progress);
    assert(execution.findOutputChannel(controller, "correction")->size() == 0);
    MediaBufferRef forwardedFlush;
    assert(execution.findOutputChannel(controller, "audio")->tryPop(
        forwardedFlush));
    assert(forwardedFlush == flush);

    auto second = boundAudio(480, 960, origin, 2);
    assert(clock->now().value() == ms(505));
    assert(execution.findOutputChannel(controller, "audio")->push(second));
    assert(execution.findInputChannel(controller, "audio")->push(second));
    const auto secondBlocked = node->process(execution);
    assert(secondBlocked && secondBlocked.value().state ==
                                MediaNodeProcessState::Waiting);
    auto purge = node->generationPurgeTarget();
    assert(purge && purge->purge({1, 2, 1}));
    MediaBufferRef secondBlocker;
    assert(execution.findOutputChannel(controller, "audio")->tryPop(
        secondBlocker));
    const auto afterPurge = node->process(execution);
    assert(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
    assert(execution.findOutputChannel(controller, "correction")->size() == 0);
    assert(execution.findOutputChannel(controller, "audio")->size() == 0);
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    assert(eof && execution.findInputChannel(controller, "audio")->push(
                      eof.value()));
    const auto finished = node->process(execution);
    assert(finished && finished.value().state == MediaNodeProcessState::Finished);
    assert(execution.findOutputChannel(controller, "correction")->size() == 0);
    MediaBufferRef forwardedEof;
    assert(execution.findOutputChannel(controller, "audio")->tryPop(
        forwardedEof));
    assert(forwardedEof == eof.value());
}

void canonicalizerCommitsContinuityOnlyAfterOutputAndPurgesGeneration()
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto canonicalizer = graph.addNode(
        MediaNodeKind::EncodedAudioCanonicalizer, "canonicalizer");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
    graph.addOutputPort(source, "encoded", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(canonicalizer, "encoded", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(canonicalizer, "canonical", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(sink, "canonical", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    assert(graph.connect(source, "encoded", canonicalizer, "encoded", "encoded",
                         MediaBlockingEdgePolicyPlanner::planQueue(2)));
    assert(graph.connect(canonicalizer, "canonical", sink, "canonical", "canonical",
                         MediaBlockingEdgePolicyPlanner::planQueue(1)));
    MediaGraphExecutionContext execution;
    assert(execution.compile(graph));
    MediaEncodedAudioCanonicalizerNode node(canonicalizer);
    assert(node.start(execution));

    const MediaAudioPlaybackOrigin firstOrigin{
        1, ms(100), ms(500), 4'800, 48'000};
    auto first = encoded(
        {{lineage(100, 10, 1, 1), {4'800, 5'280, 48'000}}}, firstOrigin);
    auto blocker = packet();
    assert(execution.findOutputChannel(canonicalizer, "canonical")->push(blocker));
    assert(execution.findInputChannel(canonicalizer, "encoded")->push(first));
    const auto blocked = node.process(execution);
    assert(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    MediaBufferRef discarded;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(discarded));
    assert(node.process(execution));
    MediaBufferRef firstOutput;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
        firstOutput));
    const auto* firstCanonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
        firstOutput.get());
    assert(firstCanonical && firstCanonical->sourceSequence() ==
                                 MediaSourceAccessUnitSequence(1));

    auto second = encoded(
        {{lineage(110, 10, 1, 2), {5'280, 5'760, 48'000}}}, firstOrigin);
    assert(execution.findInputChannel(canonicalizer, "encoded")->push(second));
    assert(node.process(execution));
    MediaBufferRef secondOutput;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
        secondOutput));
    const auto* secondCanonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
        secondOutput.get());
    assert(secondCanonical && secondCanonical->sourceSequence() ==
                                  MediaSourceAccessUnitSequence(2));

    auto purge = node.generationPurgeTarget();
    assert(purge && purge->purge({1, 2, 1}));
    const MediaAudioPlaybackOrigin secondOrigin{
        2, ms(200), ms(700), 0, 48'000};
    auto nextGeneration = encoded(
        {{lineage(200, 10, 2, 1), {0, 480, 48'000}}}, secondOrigin);
    assert(execution.findInputChannel(canonicalizer, "encoded")->push(
        nextGeneration));
    assert(node.process(execution));
    MediaBufferRef nextOutput;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
        nextOutput));
    const auto* nextCanonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
        nextOutput.get());
    assert(nextCanonical && nextCanonical->generation() == 2 &&
           nextCanonical->sourceSequence() == MediaSourceAccessUnitSequence(1));

    auto flush = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    assert(execution.findInputChannel(canonicalizer, "encoded")->push(flush));
    const auto flushed = node.process(execution);
    assert(flushed && flushed.value().state == MediaNodeProcessState::Progress);
    MediaBufferRef forwardedFlush;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
        forwardedFlush));
    assert(forwardedFlush == flush);
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    assert(eof && execution.findInputChannel(canonicalizer, "encoded")->push(
                      eof.value()));
    const auto finished = node.process(execution);
    assert(finished && finished.value().state == MediaNodeProcessState::Finished);
    MediaBufferRef terminal;
    assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
        terminal));
    assert(terminal == eof.value());
}

void audioControlNodesHonorAbortAndRequiredInputTermination()
{
    for (int mode = 0; mode != 3; ++mode) {
        MediaGraph graph;
        const auto scheduler = graph.addNode(MediaNodeKind::AvOutputScheduler, "scheduler");
        const auto binder = graph.addNode(MediaNodeKind::PlaybackEpochBinder, "binder");
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto controller = graph.addNode(
            MediaNodeKind::AudioDriftController, "controller");
        const auto correctionSink = graph.addNode(MediaNodeKind::DebugDump, "correction");
        const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio");
        graph.setNodeOption(scheduler, "av_scheduler.sync_group", "terminal-group");
        graph.setNodeOption(binder, "playback_epoch_binder.sync_group", "terminal-group");
        graph.setNodeOption(controller, "audio_drift_controller.sync_group", "terminal-group");
        graph.addOutputPort(source, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(controller, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addOutputPort(controller, "correction", MediaStreamKind::Audio,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(controller, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(correctionSink, "correction", MediaStreamKind::Audio,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(audioSink, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
        assert(graph.connect(source, "audio", controller, "audio", "audio", policy));
        assert(graph.connect(controller, "correction", correctionSink, "correction",
                             "correction", policy));
        assert(graph.connect(controller, "audio", audioSink, "audio", "audio", policy));
        MediaGraphExecutionContext execution;
        std::unique_ptr<MediaGraphRuntime> runtime;
        auto clock = std::make_shared<TestMasterClock>(ms(500));
        assert(media_transcode::test::compileAndActivateAvSyncRuntime(
            std::move(graph),
            {MediaAvSyncGroupKey("terminal-group"), completePlan(),
             MediaAvGenerationTransitionPlanner::plan(
                 MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
                 ms(1'000), ms(1'000)),
             MediaAvSyncBindingAssemblyMode::ComponentCore},
            clock, {ms(100), ms(500), 1}, binder, execution, runtime));
        auto* node = dynamic_cast<MediaAudioDriftControllerNode*>(
            runtime->scheduler().findNode(controller));
        assert(node && node->start(execution));
        MediaChannel* input = execution.findInputChannel(controller, "audio");
        if (mode == 2) {
            const MediaAudioPlaybackOrigin origin{1, ms(100), ms(500), 0, 48'000};
            assert(input->push(boundAudio(0, 480, origin, 1)));
            assert(node->process(execution));
            MediaBufferRef primed;
            execution.findOutputChannel(controller, "correction")->tryPop(primed);
            assert(execution.findOutputChannel(controller, "audio")->tryPop(primed));
            auto abort = makeMediaBufferRef<MediaControlBuffer>(
                MediaControlBufferKind::Abort);
            assert(input->push(abort));
            const auto finished = node->process(execution);
            assert(finished && finished.value().state ==
                                   MediaNodeProcessState::Finished);
            assert(execution.findOutputChannel(controller, "correction")->size() == 0);
            MediaBufferRef output;
            assert(execution.findOutputChannel(controller, "audio")->tryPop(output));
            assert(output == abort);
        } else {
            if (mode == 0) input->close();
            else input->abort();
            const auto failed = node->process(execution);
            assert(!failed && failed.error().code == ::media::ErrorCode::Cancelled);
            const auto repeated = node->process(execution);
            assert(!repeated && repeated.error().message == failed.error().message);
        }
    }

    for (int mode = 0; mode != 3; ++mode) {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto canonicalizer = graph.addNode(
            MediaNodeKind::EncodedAudioCanonicalizer, "canonicalizer");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        graph.addOutputPort(source, "encoded", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(canonicalizer, "encoded", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(canonicalizer, "canonical", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(sink, "canonical", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
        assert(graph.connect(source, "encoded", canonicalizer, "encoded", "encoded",
                             policy));
        assert(graph.connect(canonicalizer, "canonical", sink, "canonical",
                             "canonical", policy));
        MediaGraphExecutionContext execution;
        assert(execution.compile(graph));
        MediaEncodedAudioCanonicalizerNode node(canonicalizer);
        assert(node.start(execution));
        MediaChannel* input = execution.findInputChannel(canonicalizer, "encoded");
        if (mode == 2) {
            const MediaAudioPlaybackOrigin origin{1, ms(100), ms(500), 0, 48'000};
            assert(input->push(encoded(
                {{lineage(100, 10, 1, 1), {0, 480, 48'000}}}, origin)));
            assert(node.process(execution));
            MediaBufferRef primed;
            assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
                primed));
            auto abort = makeMediaBufferRef<MediaControlBuffer>(
                MediaControlBufferKind::Abort);
            assert(input->push(abort));
            const auto finished = node.process(execution);
            assert(finished && finished.value().state ==
                                   MediaNodeProcessState::Finished);
            MediaBufferRef output;
            assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
                output));
            assert(output == abort);
        } else {
            if (mode == 0) input->close();
            else input->abort();
            const auto failed = node.process(execution);
            assert(!failed && failed.error().code == ::media::ErrorCode::Cancelled);
            const auto repeated = node.process(execution);
            assert(!repeated && repeated.error().message == failed.error().message);
        }
    }
}

void generationPurgeCancelsBackpressuredAudioControls()
{
    const auto terminalControl = [](MediaControlBufferKind kind) {
        if (kind == MediaControlBufferKind::Eof) {
            auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
            assert(eof);
            return eof.value();
        }
        return MediaBufferRef(makeMediaBufferRef<MediaControlBuffer>(kind));
    };
    for (const auto terminalKind : {MediaControlBufferKind::Eof,
                                    MediaControlBufferKind::Flush,
                                    MediaControlBufferKind::Abort}) {
        MediaGraph graph;
        const auto scheduler = graph.addNode(MediaNodeKind::AvOutputScheduler, "scheduler");
        const auto binder = graph.addNode(MediaNodeKind::PlaybackEpochBinder, "binder");
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto controller = graph.addNode(
            MediaNodeKind::AudioDriftController, "controller");
        const auto correctionSink = graph.addNode(MediaNodeKind::DebugDump, "correction");
        const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio");
        graph.setNodeOption(scheduler, "av_scheduler.sync_group", "purge-group");
        graph.setNodeOption(binder, "playback_epoch_binder.sync_group", "purge-group");
        graph.setNodeOption(controller, "audio_drift_controller.sync_group", "purge-group");
        graph.addOutputPort(source, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(controller, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addOutputPort(controller, "correction", MediaStreamKind::Audio,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(controller, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(correctionSink, "correction", MediaStreamKind::Audio,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(audioSink, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        const auto inputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(2);
        const auto outputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
        assert(graph.connect(source, "audio", controller, "audio", "audio", inputPolicy));
        assert(graph.connect(controller, "correction", correctionSink, "correction",
                             "correction", outputPolicy));
        assert(graph.connect(controller, "audio", audioSink, "audio", "audio", outputPolicy));
        MediaGraphExecutionContext execution;
        std::unique_ptr<MediaGraphRuntime> runtime;
        assert(media_transcode::test::compileAndActivateAvSyncRuntime(
            std::move(graph),
            {MediaAvSyncGroupKey("purge-group"), completePlan(),
             MediaAvGenerationTransitionPlanner::plan(
                 MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
                 ms(1'000), ms(1'000)),
             MediaAvSyncBindingAssemblyMode::ComponentCore},
            std::make_shared<TestMasterClock>(ms(500)),
            {ms(100), ms(500), 1}, binder, execution, runtime));
        auto* node = dynamic_cast<MediaAudioDriftControllerNode*>(
            runtime->scheduler().findNode(controller));
        assert(node && node->start(execution));
        const MediaAudioPlaybackOrigin origin{1, ms(100), ms(500), 0, 48'000};
        assert(execution.findInputChannel(controller, "audio")->push(
            boundAudio(0, 480, origin, 1)));
        assert(node->process(execution));
        MediaBufferRef discarded;
        execution.findOutputChannel(controller, "correction")->tryPop(discarded);
        assert(execution.findOutputChannel(controller, "audio")->tryPop(discarded));
        auto blocker = boundAudio(480, 960, origin, 2);
        assert(execution.findOutputChannel(controller, "audio")->push(blocker));
        const auto terminal = terminalControl(terminalKind);
        assert(execution.findInputChannel(controller, "audio")->push(terminal));
        const auto retained = node->process(execution);
        const auto retainedState = terminalKind == MediaControlBufferKind::Flush
            ? MediaNodeProcessState::Waiting
            : MediaNodeProcessState::Progress;
        assert(retained && retained.value().state == retainedState);
        assert(node->generationPurgeTarget()->purge({1, 2, 1}));
        assert(execution.findOutputChannel(controller, "audio")->tryPop(discarded));
        const auto afterPurge = node->process(execution);
        assert(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
        assert(!execution.findOutputChannel(controller, "audio")->tryPop(discarded));
        assert(!execution.findOutputChannel(controller, "audio")->closed());
    }

    for (const auto terminalKind : {MediaControlBufferKind::Eof,
                                    MediaControlBufferKind::Flush,
                                    MediaControlBufferKind::Abort}) {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto canonicalizer = graph.addNode(
            MediaNodeKind::EncodedAudioCanonicalizer, "canonicalizer");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        graph.addOutputPort(source, "encoded", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(canonicalizer, "encoded", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(canonicalizer, "canonical", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(sink, "canonical", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        assert(graph.connect(source, "encoded", canonicalizer, "encoded", "encoded",
                             MediaBlockingEdgePolicyPlanner::planQueue(2)));
        assert(graph.connect(canonicalizer, "canonical", sink, "canonical", "canonical",
                             MediaBlockingEdgePolicyPlanner::planQueue(1)));
        MediaGraphExecutionContext execution;
        assert(execution.compile(graph));
        MediaEncodedAudioCanonicalizerNode node(canonicalizer);
        assert(node.start(execution));
        const MediaAudioPlaybackOrigin origin{1, ms(100), ms(500), 0, 48'000};
        assert(execution.findInputChannel(canonicalizer, "encoded")->push(
            encoded({{lineage(100, 10, 1, 1), {0, 480, 48'000}}}, origin)));
        assert(node.process(execution));
        MediaBufferRef discarded;
        assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
            discarded));
        auto blocker = packet();
        assert(execution.findOutputChannel(canonicalizer, "canonical")->push(blocker));
        const auto terminal = terminalControl(terminalKind);
        assert(execution.findInputChannel(canonicalizer, "encoded")->push(terminal));
        const auto retained = node.process(execution);
        const auto retainedState = terminalKind == MediaControlBufferKind::Flush
            ? MediaNodeProcessState::Waiting
            : MediaNodeProcessState::Progress;
        assert(retained && retained.value().state == retainedState);
        assert(node.generationPurgeTarget()->purge({1, 2, 1}));
        assert(execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
            discarded));
        const auto afterPurge = node.process(execution);
        assert(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
        assert(!execution.findOutputChannel(canonicalizer, "canonical")->tryPop(
            discarded));
        assert(!execution.findOutputChannel(canonicalizer, "canonical")->closed());
    }
}

void controllerFactoryRequiresPlannedSyncGroup()
{
    MediaGraph graph;
    const auto controller = graph.addNode(
        MediaNodeKind::AudioDriftController, "controller");
    const auto* planned = graph.findNode(controller);
    assert(planned);
    const auto missing = MediaRuntimeNodeFactory::create(*planned);
    assert(!missing && missing.error().code == ::media::ErrorCode::InvalidArgument);

    MediaRealtimeExecutableGraph executable;
    executable.graph.addNode(
        MediaNodeKind::AudioDriftController, "controller");
    const auto invalid = MediaGraphRuntimeCompiler::validateBindings(executable);
    assert(!invalid && invalid.error().code == ::media::ErrorCode::NotInitialized);
}

void synchronizedAudioControlsRequireObservedGeneration()
{
    {
        MediaGraph graph;
        const auto scheduler = graph.addNode(
            MediaNodeKind::AvOutputScheduler, "scheduler");
        const auto binder = graph.addNode(
            MediaNodeKind::PlaybackEpochBinder, "binder");
        const auto controller = graph.addNode(
            MediaNodeKind::AudioDriftController, "controller");
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto correction = graph.addNode(MediaNodeKind::DebugDump, "correction");
        const auto audio = graph.addNode(MediaNodeKind::DebugDump, "audio");
        graph.setNodeOption(scheduler, "av_scheduler.sync_group", "pre-media");
        graph.setNodeOption(binder, "playback_epoch_binder.sync_group", "pre-media");
        graph.setNodeOption(
            controller, "audio_drift_controller.sync_group", "pre-media");
        graph.addOutputPort(source, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(controller, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addOutputPort(controller, "correction", MediaStreamKind::Audio,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(controller, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        graph.addInputPort(correction, "correction", MediaStreamKind::Audio,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(audio, "audio", MediaStreamKind::Audio,
                           MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(1);
        assert(graph.connect(source, "audio", controller, "audio", "audio", policy));
        assert(graph.connect(controller, "correction", correction, "correction",
                             "correction", policy));
        assert(graph.connect(controller, "audio", audio, "audio", "audio", policy));
        MediaGraphExecutionContext execution;
        std::unique_ptr<MediaGraphRuntime> runtime;
        assert(media_transcode::test::compileAndActivateAvSyncRuntime(
            std::move(graph),
            {MediaAvSyncGroupKey("pre-media"), completePlan(),
             MediaAvGenerationTransitionPlanner::plan(
                 MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
                 ms(1'000), ms(1'000)),
             MediaAvSyncBindingAssemblyMode::ComponentCore},
            std::make_shared<TestMasterClock>(ms(500)),
            {ms(100), ms(500), 1}, binder, execution, runtime));
        auto* node = dynamic_cast<MediaAudioDriftControllerNode*>(
            runtime->scheduler().findNode(controller));
        assert(node && node->start(execution));
        assert(execution.findInputChannel(controller, "audio")->push(
            makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Abort)));
        const auto rejected = node->process(execution);
        assert(!rejected && rejected.error().code == ::media::ErrorCode::InvalidArgument);
    }

    {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto canonicalizer = graph.addNode(
            MediaNodeKind::EncodedAudioCanonicalizer, "canonicalizer");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        graph.addOutputPort(source, "encoded", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(canonicalizer, "encoded", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(canonicalizer, "canonical", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(sink, "canonical", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(1);
        assert(graph.connect(source, "encoded", canonicalizer, "encoded", "encoded",
                             policy));
        assert(graph.connect(canonicalizer, "canonical", sink, "canonical",
                             "canonical", policy));
        MediaGraphExecutionContext execution;
        assert(execution.compile(graph));
        MediaEncodedAudioCanonicalizerNode node(canonicalizer);
        assert(node.start(execution));
        assert(execution.findInputChannel(canonicalizer, "encoded")->push(
            makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Abort)));
        const auto rejected = node.process(execution);
        assert(!rejected && rejected.error().code == ::media::ErrorCode::InvalidArgument);
    }
}

} // namespace

int main()
{
    canonicalizerUsesFragmentsNotPacketPts();
    canonicalizerRejectsGapOverlapAndGenerationMismatch();
    driftMeasurementUsesActiveEpochAndCanonicalAudioEnd();
    controllerRetainsAtomicCorrectionBeforeAudioAcrossBackpressure();
    canonicalizerCommitsContinuityOnlyAfterOutputAndPurgesGeneration();
    audioControlNodesHonorAbortAndRequiredInputTermination();
    generationPurgeCancelsBackpressuredAudioControls();
    controllerFactoryRequiresPlannedSyncGroup();
    synchronizedAudioControlsRequireObservedGeneration();
    return 0;
}
