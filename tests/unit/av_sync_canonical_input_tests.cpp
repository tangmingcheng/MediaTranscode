#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"
#include "internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>

using namespace media::ffmpeg::graph;

#undef assert
#define assert(condition)                                                        \
    do {                                                                         \
        if (!(condition)) {                                                      \
            std::fprintf(stderr, "check failed: %s:%d: %s\n",                 \
                         __FILE__, __LINE__, #condition);                         \
            std::exit(1);                                                        \
        }                                                                        \
    } while (false)

namespace {

void initialClockAcquisitionDeadlineIsOneShotAndExact()
{
    auto created = MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(100));
    assert(created);
    auto deadline = std::move(created).value();
    assert(!deadline.deadline());
    assert(deadline.establish(MediaRunningTime::fromNanoseconds(5)));
    assert(deadline.deadline() ==
           std::optional<MediaRunningTime>(
               MediaRunningTime::fromNanoseconds(105)));
    assert(deadline.establish(MediaRunningTime::fromNanoseconds(10)));
    assert(deadline.deadline() ==
           std::optional<MediaRunningTime>(
               MediaRunningTime::fromNanoseconds(105)));
    assert(deadline.preflight(MediaRunningTime::fromNanoseconds(104)));
    assert(!deadline.preflight(MediaRunningTime::fromNanoseconds(105)));
    deadline.clear();
    assert(!deadline.deadline());
    assert(deadline.preflight(MediaRunningTime::fromNanoseconds(105)));

    assert(!MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(0)));
    auto overflow = MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(100));
    assert(overflow);
    assert(!overflow.value().establish(MediaRunningTime::fromNanoseconds(
        std::numeric_limits<std::int64_t>::max() - 99)));
}

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(std::int64_t now) : m_now(now) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(m_now.load()));
    }

    void set(std::int64_t now) noexcept { m_now.store(now); }

private:
    std::atomic<std::int64_t> m_now;
};

MediaAvSyncPlan completeAvSyncPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "ts-gate-tests";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 8;
    request.avSyncStartup.maximumVideoUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = MediaRunningTime::fromNanoseconds(40'000'000);
    auto planned = MediaAvSyncPlanner::plan(request);
    assert(planned);
    auto plan = std::move(planned).value();
    plan.audioServo.commandLeadNs = MediaRunningTime::fromNanoseconds(1'500'000'000);
    plan.audioServo.compensationWindowNs = MediaRunningTime::fromNanoseconds(2'000'000'000);
    plan.audioServo.frequencyFilterTimeConstantNs = MediaRunningTime::fromNanoseconds(5'000'000'000);
    return plan;
}

void registerSyncGroup(MediaGraphExecutionContext& execution,
                       const std::shared_ptr<TestMasterClock>& clock)
{
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::nanoseconds(0));
    assert(epoch);
    auto transition = MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    auto service = MediaAvEpochTransitionService::create(std::move(transition));
    assert(service);
    assert(execution.registerAvSyncGroup(
        MediaAvSyncGroupKey("ts-gate-group"), completeAvSyncPlan(), clock,
        std::make_shared<MediaSharedNtpEpoch>(std::move(epoch).value()),
        std::move(service).value()));
}

MediaBufferRef timedPacket(MediaStreamKind stream,
                           MediaSourceClockReadiness readiness,
                           std::uint64_t generation,
                           std::int64_t presentationNs,
                           std::int64_t duration,
                           AVRational timeBase,
                           bool discontinuity = false)
{
    auto packet = ::media::ffmpeg::makePacket();
    assert(packet);
    assert(av_new_packet(packet.get(), 16) == 0);
    packet->pts = 100;
    packet->dts = 90;
    packet->duration = duration;
    packet->time_base = timeBase;
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), stream,
        MediaPacketSourceTiming{presentationNs, presentationNs - 10,
                                readiness, generation});
    assert(wrapped);
    wrapped.value()->setTimeDescriptor(
        MediaTimeDescriptor{MediaRational{timeBase.num, timeBase.den}});
    if (discontinuity) wrapped.value()->addFlags(MediaBufferFlag::Discontinuity);
    return std::move(wrapped).value();
}

MediaBufferRef packetWithoutTiming(MediaStreamKind stream)
{
    auto packet = ::media::ffmpeg::makePacket();
    assert(packet);
    assert(av_new_packet(packet.get(), 16) == 0);
    packet->pts = 100;
    packet->dts = 90;
    packet->duration = 3'600;
    packet->time_base = AVRational{1, 90'000};
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), stream, std::nullopt);
    assert(wrapped);
    return std::move(wrapped).value();
}

struct GateHarness final {
    MediaGraph graph;
    MediaNodeId gate;
    MediaGraphExecutionContext execution;
    std::shared_ptr<TestMasterClock> clock = std::make_shared<TestMasterClock>(0);
    std::unique_ptr<MediaInitialLockedPacketGateNode> runtime;

    explicit GateHarness(std::int64_t timeoutNs = 100)
    {
        const auto packetSource = graph.addNode(MediaNodeKind::DebugDump, "packet-source");
        const auto clockSource = graph.addNode(MediaNodeKind::DebugDump, "clock-source");
        gate = graph.addNode(MediaNodeKind::InitialLockedPacketGate, "initial-lock");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        const auto queue = MediaGraphBuildSupport::blockingQueuePolicy(8);
        graph.addOutputPort(packetSource, "out", MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(clockSource, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(gate, "packet", MediaStreamKind::Video,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(gate, "clock", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(gate, "packet", MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(sink, "in", MediaStreamKind::Video,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.connect(packetSource, "out", gate, "packet", "packet", queue);
        graph.connect(clockSource, "out", gate, "clock", "clock", queue);
        graph.connect(gate, "packet", sink, "in", "output", queue);
        graph.setNodeOption(gate, "initial_locked_gate.stream", "video");
        graph.setNodeOption(gate, "initial_locked_gate.acquiring_timeout_ns",
                            std::to_string(timeoutNs));
        graph.setNodeOption(gate, "initial_locked_gate.sync_group", "ts-gate-group");
        assert(execution.compile(graph));
        registerSyncGroup(execution, clock);
        runtime = std::make_unique<MediaInitialLockedPacketGateNode>(gate);
        assert(runtime->start(execution));
    }

    MediaChannel* packetInput() { return execution.findInputChannel(gate, "packet"); }
    MediaChannel* clockInput() { return execution.findInputChannel(gate, "clock"); }
    MediaChannel* output() { return execution.findOutputChannel(gate, "packet"); }
};

void typedClockStateIsImmutableAndDiagnostic()
{
    const MediaSourceClockStateBuffer state(
        MediaSourceClockReadiness::Locked, 7, false);
    assert(state.readiness() == MediaSourceClockReadiness::Locked);
    assert(state.generation() == 7);
    assert(state.streamKind() == MediaStreamKind::Metadata);
    assert(state.payloadKind() == MediaPayloadKind::GraphEvent);
    assert(state.diagnosticName() == "source.clock_state");
    const MediaSourceClockStateBuffer discontinuity(
        MediaSourceClockReadiness::ReacquireRequired, 8, true);
    assert(hasFlag(discontinuity.flags(), MediaBufferFlag::Discontinuity));
}

void gateRequiresLockedClockBeforeLockedPackets()
{
    GateHarness harness;
    assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
        MediaSourceClockReadiness::Acquiring, 0, false)));
    assert(harness.runtime->process(harness.execution));
    assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
        MediaSourceClockReadiness::Locked, 7, false)));
    assert(harness.runtime->process(harness.execution));

    auto packet = timedPacket(MediaStreamKind::Video,
                              MediaSourceClockReadiness::Locked, 7,
                              1'000, 3'600, AVRational{1, 90'000});
    assert(harness.packetInput()->push(packet));
    assert(harness.runtime->process(harness.execution));
    MediaBufferRef released;
    assert(harness.output()->tryPop(released));
    assert(released.get() == packet.get());

    assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
        MediaSourceClockReadiness::Locked, 8, false)));
    assert(!harness.runtime->process(harness.execution));
}

void gateRejectsPacketsBeforeLockAndInvalidEvidence()
{
    {
        GateHarness harness;
        assert(harness.packetInput()->push(timedPacket(
            MediaStreamKind::Video, MediaSourceClockReadiness::Locked, 7,
            1'000, 3'600, AVRational{1, 90'000})));
        assert(!harness.runtime->process(harness.execution));
    }
    {
        GateHarness harness;
        assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::Locked, 0, false)));
        assert(!harness.runtime->process(harness.execution));
    }
    for (const auto readiness : {MediaSourceClockReadiness::Degraded,
                                 MediaSourceClockReadiness::ReacquireRequired}) {
        GateHarness harness;
        assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
            readiness, 7, false)));
        assert(!harness.runtime->process(harness.execution));
    }
    {
        GateHarness harness;
        assert(harness.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::Locked, 7, true)));
        assert(!harness.runtime->process(harness.execution));
    }
    {
        GateHarness harness;
        assert(harness.packetInput()->push(
            packetWithoutTiming(MediaStreamKind::Video)));
        assert(!harness.runtime->process(harness.execution));
    }
}

void gateDeadlinePreflightPrecedesAllQueuedEvidence()
{
    {
        GateHarness idle(100);
        assert(idle.clockInput()->push(makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::Acquiring, 0, false)));
        assert(idle.runtime->process(idle.execution));
        auto waiting = idle.runtime->process(idle.execution);
        assert(waiting && waiting.value().deadlineWait);
        assert(waiting.value().deadlineWait->syncGroup ==
               MediaAvSyncGroupKey("ts-gate-group"));
        assert(waiting.value().deadlineWait->masterDeadline ==
               MediaRunningTime::fromNanoseconds(100));
    }
    {
        GateHarness lateLocked(100);
        assert(lateLocked.clockInput()->push(
            makeMediaBufferRef<MediaSourceClockStateBuffer>(
                MediaSourceClockReadiness::Acquiring, 0, false)));
        assert(lateLocked.runtime->process(lateLocked.execution));
        assert(lateLocked.clockInput()->push(
            makeMediaBufferRef<MediaSourceClockStateBuffer>(
                MediaSourceClockReadiness::Locked, 7, false)));
        lateLocked.clock->set(100);
        assert(!lateLocked.runtime->process(lateLocked.execution));
    }
    {
        GateHarness acquiring(100);
        assert(acquiring.clockInput()->push(
            makeMediaBufferRef<MediaSourceClockStateBuffer>(
                MediaSourceClockReadiness::Acquiring, 0, false)));
        assert(acquiring.runtime->process(acquiring.execution));
        assert(acquiring.clockInput()->push(
            makeMediaBufferRef<MediaSourceClockStateBuffer>(
                MediaSourceClockReadiness::Acquiring, 0, false)));
        acquiring.clock->set(99);
        assert(acquiring.runtime->process(acquiring.execution));
        assert(acquiring.clockInput()->push(
            makeMediaBufferRef<MediaSourceClockStateBuffer>(
                MediaSourceClockReadiness::Acquiring, 0, false)));
        acquiring.clock->set(100);
        assert(!acquiring.runtime->process(acquiring.execution));
    }
}

struct CanonicalHarness final {
    MediaGraph graph;
    MediaNodeId node;
    MediaGraphExecutionContext execution;
    std::unique_ptr<MediaCanonicalInputNode> runtime;

    CanonicalHarness(MediaStreamKind stream, const char* durationSource,
                     int sampleRate = 0, int sampleCount = 0)
    {
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        node = graph.addNode(MediaNodeKind::CanonicalInput, "canonical");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        const auto queue = MediaGraphBuildSupport::blockingQueuePolicy(4);
        graph.addOutputPort(source, "out", stream, MediaEdgeKind::EncodedPacket,
                            MediaPayloadKind::Packet);
        graph.addInputPort(node, "in", stream, MediaEdgeKind::EncodedPacket,
                           MediaPayloadKind::Packet);
        graph.addOutputPort(node, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.connect(source, "out", node, "in", "input", queue);
        graph.connect(node, "out", sink, "in", "output", queue);
        graph.setNodeOption(node, "canonical_input.stream",
                            stream == MediaStreamKind::Video ? "video" : "audio");
        graph.setNodeOption(node, "canonical_input.source_identity",
                            stream == MediaStreamKind::Video ? "video-source" : "audio-source");
        graph.setNodeOption(node, "canonical_input.decode_order",
                            stream == MediaStreamKind::Video ? "reordered" : "presentation");
        graph.setNodeOption(node, "canonical_input.duration_source", durationSource);
        if (sampleRate > 0) {
            graph.setNodeOption(node, "canonical_input.audio_sample_rate",
                                std::to_string(sampleRate));
            graph.setNodeOption(node, "canonical_input.audio_sample_count",
                                std::to_string(sampleCount));
        }
        assert(execution.compile(graph));
        runtime = std::make_unique<MediaCanonicalInputNode>(node);
        assert(runtime->start(execution));
    }

    MediaChannel* input() { return execution.findInputChannel(node, "in"); }
    MediaChannel* output() { return execution.findOutputChannel(node, "out"); }
};

const MediaAvStartupEnvelopeBuffer* canonicalizeOne(
    CanonicalHarness& harness, MediaBufferRef packet, MediaBufferRef& owner)
{
    assert(harness.input()->push(std::move(packet)));
    assert(harness.runtime->process(harness.execution));
    assert(harness.output()->tryPop(owner));
    return dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(owner.get());
}

void canonicalInputUsesRuntimeGenerationAndRtpTsPacketDuration()
{
    CanonicalHarness rtp(MediaStreamKind::Video, "packet");
    MediaBufferRef owner;
    const auto* envelope = canonicalizeOne(
        rtp,
        timedPacket(MediaStreamKind::Video, MediaSourceClockReadiness::Locked,
                    7, 1'000, 3'600, AVRational{1, 90'000}),
        owner);
    assert(envelope);
    assert(envelope->unit().generation == 7);
    assert(envelope->unit().presentationTime);
    assert(envelope->unit().presentationTime->nanoseconds() == 1'000);
    assert(envelope->unit().duration.nanoseconds() == 40'000'000);

    CanonicalHarness mpegTs(MediaStreamKind::Audio, "packet", 48'000);
    MediaBufferRef tsOwner;
    const auto* tsEnvelope = canonicalizeOne(
        mpegTs,
        timedPacket(MediaStreamKind::Audio, MediaSourceClockReadiness::Locked,
                    11, 2'000, 1'024, AVRational{1, 48'000}),
        tsOwner);
    assert(tsEnvelope);
    assert(tsEnvelope->unit().generation == 11);
    assert(tsEnvelope->unit().duration.nanoseconds() == 21'333'333);

    CanonicalHarness inexactAudio(MediaStreamKind::Audio, "packet", 48'000);
    assert(inexactAudio.input()->push(timedPacket(
        MediaStreamKind::Audio, MediaSourceClockReadiness::Locked,
        11, 3'000, 1, AVRational{1, 90'000})));
    assert(!inexactAudio.runtime->process(inexactAudio.execution));
}

void canonicalInputUsesExactAudioSampleSpanAndRejectsInvalidTiming()
{
    CanonicalHarness harness(MediaStreamKind::Audio, "audio_samples", 48'000, 960);
    MediaBufferRef owner;
    const auto* envelope = canonicalizeOne(
        harness,
        timedPacket(MediaStreamKind::Audio, MediaSourceClockReadiness::Locked,
                    9, 2'000, 0, AVRational{1, 48'000}),
        owner);
    assert(envelope);
    assert(envelope->unit().generation == 9);
    assert(envelope->unit().duration.nanoseconds() == 20'000'000);
    assert(envelope->unit().audio);
    assert(envelope->unit().audio->sampleRate == 48'000);
    assert(envelope->unit().audio->sampleCount == 960);

    CanonicalHarness invalidSampleRate(
        MediaStreamKind::Audio, "audio_samples", 0, 960);
    assert(!invalidSampleRate.runtime->process(invalidSampleRate.execution));
    assert(!invalidSampleRate.runtime->process(invalidSampleRate.execution));
    CanonicalHarness invalidSampleCount(
        MediaStreamKind::Audio, "audio_samples", 48'000, 0);
    assert(!invalidSampleCount.runtime->process(invalidSampleCount.execution));

    CanonicalHarness invalid(MediaStreamKind::Video, "packet");
    assert(invalid.input()->push(timedPacket(
        MediaStreamKind::Video, MediaSourceClockReadiness::Acquiring,
        0, 1'000, 3'600, AVRational{1, 90'000})));
    assert(!invalid.runtime->process(invalid.execution));

    CanonicalHarness absent(MediaStreamKind::Video, "packet");
    assert(absent.input()->push(packetWithoutTiming(MediaStreamKind::Video)));
    assert(!absent.runtime->process(absent.execution));

    CanonicalHarness noRuntimeTimeBase(MediaStreamKind::Video, "packet");
    auto packet = timedPacket(MediaStreamKind::Video,
                              MediaSourceClockReadiness::Locked, 7,
                              1'000, 3'600, AVRational{1, 90'000});
    auto* ffmpegPacket = dynamic_cast<FFmpegPacketBuffer*>(packet.get());
    assert(ffmpegPacket);
    ffmpegPacket->packet()->time_base = AVRational{0, 1};
    assert(noRuntimeTimeBase.input()->push(std::move(packet)));
    assert(!noRuntimeTimeBase.runtime->process(noRuntimeTimeBase.execution));
}

void canonicalInputPropagatesTerminalWithoutInventingMedia()
{
    CanonicalHarness harness(MediaStreamKind::Audio, "audio_samples", 48'000, 960);
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    assert(eof);
    assert(harness.input()->push(eof.value()));
    const auto result = harness.runtime->process(harness.execution);
    assert(result);
    assert(result.value().state == MediaNodeProcessState::Finished);
    MediaBufferRef output;
    assert(harness.output()->tryPop(output));
    assert(output == eof.value());
    assert(output->isEof());
    assert(harness.output()->size() == 0);
}

void canonicalInputPropagatesExplicitAbortAndFailsClosedOnMissingTerminal()
{
    for (const auto kind : {MediaControlBufferKind::Flush,
                            MediaControlBufferKind::Abort}) {
        CanonicalHarness harness(MediaStreamKind::Audio, "audio_samples", 48'000, 960);
        auto control = makeMediaBufferRef<MediaControlBuffer>(kind);
        assert(harness.input()->push(control));
        const auto result = harness.runtime->process(harness.execution);
        assert(result && result.value().state ==
                             (kind == MediaControlBufferKind::Abort
                                  ? MediaNodeProcessState::Finished
                                  : MediaNodeProcessState::Progress));
        MediaBufferRef output;
        assert(harness.output()->tryPop(output) && output == control);
    }
    for (const bool abortChannel : {false, true}) {
        CanonicalHarness harness(MediaStreamKind::Audio, "audio_samples", 48'000, 960);
        if (abortChannel) harness.input()->abort();
        else harness.input()->close();
        const auto result = harness.runtime->process(harness.execution);
        assert(!result && result.error().code == ::media::ErrorCode::Cancelled);
        const auto repeated = harness.runtime->process(harness.execution);
        assert(!repeated && repeated.error().code == result.error().code &&
               repeated.error().message == result.error().message);
    }
}

void nodeKindFactoryAndDiagnosticsAreComplete()
{
    MediaGraph graph;
    const auto node = graph.addNode(MediaNodeKind::InitialLockedPacketGate, "gate");
    assert(MediaGraphDump::toText(graph).find("InitialLockedPacketGate") != std::string::npos);
    assert(MediaRuntimeNodeFactory::supported(MediaNodeKind::InitialLockedPacketGate));
    const auto* model = graph.findNode(node);
    assert(model);
    auto runtime = MediaRuntimeNodeFactory::create(*model);
    assert(runtime);
}

} // namespace

int main()
{
    initialClockAcquisitionDeadlineIsOneShotAndExact();
    typedClockStateIsImmutableAndDiagnostic();
    gateRequiresLockedClockBeforeLockedPackets();
    gateRejectsPacketsBeforeLockAndInvalidEvidence();
    gateDeadlinePreflightPrecedesAllQueuedEvidence();
    canonicalInputUsesRuntimeGenerationAndRtpTsPacketDuration();
    canonicalInputUsesExactAudioSampleSpanAndRejectsInvalidTiming();
    canonicalInputPropagatesTerminalWithoutInventingMedia();
    canonicalInputPropagatesExplicitAbortAndFailsClosedOnMissingTerminal();
    nodeKindFactoryAndDiagnosticsAreComplete();
    return 0;
}
