#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h"
#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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

MediaRtpSourceClockCalibration calibration(
    std::uint32_t ssrc,
    std::vector<std::uint8_t> cname,
    std::uint64_t generation,
    std::int64_t anchor,
    std::int64_t sourceNanoseconds,
    int clockRate)
{
    return MediaRtpSourceClockCalibration{
        ssrc,
        std::move(cname),
        generation,
        static_cast<std::uint32_t>(anchor),
        anchor,
        MediaRunningTime::fromNanoseconds(sourceNanoseconds),
        MediaRunningTime::fromNanoseconds(sourceNanoseconds),
        1'000'000'000,
        clockRate,
        0,
        MediaRtpSourceClockConfidence::Locked};
}

MediaRtpClockGroupSnapshot lockedSnapshot(std::uint64_t groupGeneration = 1)
{
    const std::vector<std::uint8_t> binaryCname{0x63, 0x00, 0xff};
    return MediaRtpClockGroupSnapshot{
        MediaRtpClockGroupState::Locked,
        groupGeneration,
        MediaRtpLockedClockGroup{
            MediaRunningTime::fromNanoseconds(100'000'000'000LL),
            binaryCname,
            calibration(11, binaryCname, 4, 0xfffffff0LL,
                        100'000'000'000LL, 90'000),
            calibration(22, binaryCname, 7, 48'000,
                        100'250'000'000LL, 48'000)}};
}

MediaRtcpClockEvidence evidence(std::uint32_t ssrc,
                                std::uint32_t rtpTimestamp,
                                std::uint64_t sourceGeneration,
                                std::int64_t observedAtNs)
{
    return MediaRtcpClockEvidence{
        ssrc, ssrc, ssrc, {100, 0}, rtpTimestamp,
        {0x63, 0x00, 0xff}, observedAtNs, observedAtNs,
        sourceGeneration};
}

MediaBufferRef packet(MediaStreamKind stream, std::int64_t extendedTimestamp,
                      std::int64_t duration = 0)
{
    auto value = ::media::ffmpeg::makePacket();
    assert(value);
    value->pts = extendedTimestamp;
    value->dts = extendedTimestamp;
    value->duration = duration;
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(value), stream, std::nullopt);
    assert(wrapped);
    return std::move(wrapped).value();
}

void projectionUsesOneLockedEpochAndAcceptsExtendedWrapAndReordering()
{
    const auto snapshot = lockedSnapshot();
    assert(snapshot.locked);
    assert(snapshot.locked->cname == std::vector<std::uint8_t>({0x63, 0x00, 0xff}));

    MediaRtpPacketClockProjector projector;
    auto beforeWrap = projector.project(
        snapshot, MediaScheduledStream::Video, 0xfffffff8ULL);
    auto afterWrap = projector.project(
        snapshot, MediaScheduledStream::Video, 0x100000020ULL);
    auto reordered = projector.project(
        snapshot, MediaScheduledStream::Video, 0x100000010ULL);
    auto audio = projector.project(
        snapshot, MediaScheduledStream::Audio, 48'480);
    assert(beforeWrap && afterWrap && reordered && audio);
    assert(beforeWrap.value().generation == 1);
    assert(afterWrap.value().presentationNs > beforeWrap.value().presentationNs);
    assert(reordered.value().presentationNs < afterWrap.value().presentationNs);
    assert(audio.value().presentationNs == std::optional<std::int64_t>(260'000'000));
    assert(audio.value().decodeNs == audio.value().presentationNs);
}

void registeredNodeKindsHaveStableDiagnosticNames()
{
    MediaGraph graph;
    graph.addNode(MediaNodeKind::RtpPacketClockBinder, "binder");
    graph.addNode(MediaNodeKind::RtpClockSnapshotFanout, "fanout");
    const auto dump = MediaGraphDump::toText(graph);
    assert(dump.find("RtpPacketClockBinder") != std::string::npos);
    assert(dump.find("RtpClockSnapshotFanout") != std::string::npos);
}

void validatorKeepsOneCommonEpochForAGroupGeneration()
{
    auto validator = MediaRtpClockGroupValidator::create(
        MediaRtpClockGroupValidatorConfig{
            3'000'000'000LL, 5'000'000'000LL, 50'000'000,
            5'000'000'000LL, 5'000'000'000LL});
    assert(validator);
    auto group = std::move(validator).value();
    const auto cname = std::vector<std::uint8_t>{0x63, 0x00, 0xff};

    assert(group.observe(MediaStreamKind::Video, evidence(11, 90'000, 4, 0),
                         calibration(11, cname, 4, 90'000,
                                     100'000'000'000LL, 90'000)));
    assert(group.observe(MediaStreamKind::Audio, evidence(22, 48'000, 7, 0),
                         calibration(22, cname, 7, 48'000,
                                     100'010'000'000LL, 48'000)));
    const auto first = group.snapshot(0);
    assert(first.locked);
    const auto generation = first.groupGeneration;
    const auto epoch = first.locked->commonSourceEpoch;

    assert(group.observe(MediaStreamKind::Video, evidence(11, 270'000, 4, 0),
                         calibration(11, cname, 4, 270'000,
                                     100'020'000'000LL, 90'000)));
    assert(group.observe(MediaStreamKind::Audio, evidence(22, 144'000, 7, 0),
                         calibration(22, cname, 7, 144'000,
                                     100'030'000'000LL, 48'000)));
    const auto refreshed = group.snapshot(0);
    assert(refreshed.locked);
    assert(refreshed.groupGeneration == generation);
    assert(refreshed.locked->commonSourceEpoch == epoch);
}

void malformedOrUnavailableSnapshotsFailClosed()
{
    MediaRtpPacketClockProjector projector;
    auto missing = lockedSnapshot();
    missing.locked.reset();
    assert(!projector.project(missing, MediaScheduledStream::Video, 0));

    auto extra = lockedSnapshot();
    extra.state = MediaRtpClockGroupState::Acquiring;
    assert(!projector.project(extra, MediaScheduledStream::Video, 0));

    for (const auto state : {MediaRtpClockGroupState::Acquiring,
                             MediaRtpClockGroupState::Degraded,
                             MediaRtpClockGroupState::ReacquireRequired}) {
        const MediaRtpClockGroupSnapshot unavailable{state, 0, std::nullopt};
        assert(!projector.project(
            unavailable, MediaScheduledStream::Audio, 48'000));
    }
}

struct BinderFixture final {
    MediaGraph graph;
    MediaNodeId packetSource;
    MediaNodeId clockSource;
    MediaNodeId binder;
    MediaNodeId sink;
    MediaGraphExecutionContext execution;

    BinderFixture(MediaStreamKind stream, std::size_t acquiringCapacity = 2,
                  std::size_t outputCapacity = 4)
    {
        packetSource = graph.addNode(MediaNodeKind::DebugDump, "packet.source");
        clockSource = graph.addNode(MediaNodeKind::DebugDump, "clock.source");
        binder = graph.addNode(MediaNodeKind::RtpPacketClockBinder, "clock.binder");
        sink = graph.addNode(MediaNodeKind::DebugDump, "packet.sink");
        graph.addOutputPort(packetSource, "packet", stream,
                            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(clockSource, "clock", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(binder, "packet", stream,
                           MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.addInputPort(binder, "clock", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(binder, "packet", stream,
                            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.addInputPort(sink, "packet", stream,
                           MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        assert(graph.connect(packetSource, "packet", binder, "packet", "packet.in",
                             MediaGraphBuildSupport::blockingQueuePolicy(8)));
        assert(graph.connect(clockSource, "clock", binder, "clock", "clock.in",
                             MediaGraphBuildSupport::blockingQueuePolicy(8)));
        assert(graph.connect(binder, "packet", sink, "packet", "packet.out",
                             MediaGraphBuildSupport::blockingQueuePolicy(outputCapacity)));
        assert(graph.setNodeOption(binder, "rtp_clock_binder.stream",
                                   stream == MediaStreamKind::Video ? "video" : "audio"));
        assert(graph.setNodeOption(binder, "rtp_clock_binder.acquiring_capacity",
                                   std::to_string(acquiringCapacity)));
        assert(graph.setNodeOption(binder, "rtp_clock_binder.acquiring_timeout_ns",
                                   "1000000000"));
        if (stream == MediaStreamKind::Video) {
            assert(graph.setNodeOption(binder, "rtp_clock_binder.duration_clock_rate",
                                       "90000"));
            assert(graph.setNodeOption(
                binder, "rtp_clock_binder.terminal_duration_policy",
                "repeat_last_observed_positive_delta"));
        }
        assert(execution.compile(graph));
    }

    MediaChannel* packetInput() { return execution.findInputChannel(binder, "packet"); }
    MediaChannel* clockInput() { return execution.findInputChannel(binder, "clock"); }
    MediaChannel* output() { return execution.findOutputChannel(binder, "packet"); }
};

template <typename Node>
void processSucceeds(Node& node, MediaGraphExecutionContext& execution)
{
    auto result = node.process(execution);
    assert(result);
}

const FFmpegPacketBuffer* popPacket(MediaChannel& channel, MediaBufferRef& owner)
{
    if (!channel.tryPop(owner)) return nullptr;
    return dynamic_cast<const FFmpegPacketBuffer*>(owner.get());
}

void clockAndPacketMayArriveInEitherOrderWithMatchingGeneration()
{
    BinderFixture clockFirst(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode clockFirstNode(clockFirst.binder);
    assert(clockFirst.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(clockFirstNode, clockFirst.execution);
    assert(clockFirst.packetInput()->push(packet(MediaStreamKind::Audio, 48'480, 480)));
    processSucceeds(clockFirstNode, clockFirst.execution);
    MediaBufferRef clockFirstOutput;
    const auto* clockFirstPacket = popPacket(*clockFirst.output(), clockFirstOutput);
    assert(clockFirstPacket && clockFirstPacket->sourceTiming());
    assert(clockFirstPacket->sourceTiming()->generation == 1);

    BinderFixture packetFirst(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode packetFirstNode(packetFirst.binder);
    assert(packetFirst.packetInput()->push(packet(MediaStreamKind::Audio, 48'960, 480)));
    processSucceeds(packetFirstNode, packetFirst.execution);
    MediaBufferRef absent;
    assert(!packetFirst.output()->tryPop(absent));
    assert(packetFirst.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(packetFirstNode, packetFirst.execution);
    processSucceeds(packetFirstNode, packetFirst.execution);
    const auto* packetFirstPacket = popPacket(*packetFirst.output(), absent);
    assert(packetFirstPacket && packetFirstPacket->sourceTiming());
}

void acquiringIsBoundedAndEvidenceAndGenerationChangesFailClosed()
{
    BinderFixture bounded(MediaStreamKind::Audio, 1);
    MediaRtpPacketClockBinderNode boundedNode(bounded.binder);
    assert(bounded.packetInput()->push(packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(boundedNode, bounded.execution);
    assert(bounded.packetInput()->push(packet(MediaStreamKind::Audio, 48'480, 480)));
    assert(!boundedNode.process(bounded.execution));

    for (const auto state : {MediaRtpClockGroupState::Degraded,
                             MediaRtpClockGroupState::ReacquireRequired}) {
        BinderFixture fixture(MediaStreamKind::Audio);
        MediaRtpPacketClockBinderNode node(fixture.binder);
        assert(fixture.clockInput()->push(makeMediaBufferRef<MediaRtpClockGroupBuffer>(
            MediaRtpClockGroupSnapshot{state, 1, std::nullopt})));
        assert(!node.process(fixture.execution));
    }

    for (const std::uint64_t changedGeneration : {0ULL, 2ULL}) {
        BinderFixture fixture(MediaStreamKind::Audio);
        MediaRtpPacketClockBinderNode node(fixture.binder);
        assert(fixture.clockInput()->push(
            makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot(1))));
        processSucceeds(node, fixture.execution);
        assert(fixture.clockInput()->push(
            makeMediaBufferRef<MediaRtpClockGroupBuffer>(
                lockedSnapshot(changedGeneration))));
        assert(!node.process(fixture.execution));
    }
}

void validatorMayRelockButBinderRejectsTheNewGroupGeneration()
{
    auto validator = MediaRtpClockGroupValidator::create(
        MediaRtpClockGroupValidatorConfig{
            3'000'000'000LL, 5'000'000'000LL, 50'000'000,
            5'000'000'000LL, 5'000'000'000LL});
    assert(validator);
    auto group = std::move(validator).value();
    const auto video1 = evidence(11, 90'000, 4, 0);
    const auto audio1 = evidence(22, 48'000, 7, 0);
    assert(group.observe(MediaStreamKind::Video, video1,
                         calibration(11, video1.cname, 4, 90'000,
                                     100'000'000'000LL, 90'000)));
    assert(group.observe(MediaStreamKind::Audio, audio1,
                         calibration(22, audio1.cname, 7, 48'000,
                                     100'010'000'000LL, 48'000)));
    const auto first = group.snapshot(0);
    assert(first.state == MediaRtpClockGroupState::Locked);
    assert(first.groupGeneration != 0);

    group.invalidate();
    const auto video2 = evidence(11, 180'000, 5, 0);
    const auto audio2 = evidence(22, 96'000, 8, 0);
    assert(group.observe(MediaStreamKind::Video, video2,
                         calibration(11, video2.cname, 5, 180'000,
                                     101'000'000'000LL, 90'000)));
    assert(group.observe(MediaStreamKind::Audio, audio2,
                         calibration(22, audio2.cname, 8, 96'000,
                                     101'010'000'000LL, 48'000)));
    const auto second = group.snapshot(0);
    assert(second.state == MediaRtpClockGroupState::Locked);
    assert(second.groupGeneration == first.groupGeneration + 1);

    BinderFixture fixture(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode node(fixture.binder);
    assert(fixture.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(first)));
    processSucceeds(node, fixture.execution);
    assert(fixture.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(second)));
    assert(!node.process(fixture.execution));
}

void videoDurationUsesLookaheadAndOnlyExplicitTerminalReuse()
{
    BinderFixture fixture(MediaStreamKind::Video);
    MediaRtpPacketClockBinderNode node(fixture.binder);
    assert(fixture.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(node, fixture.execution);
    assert(fixture.packetInput()->push(packet(MediaStreamKind::Video, 0x100000010LL)));
    processSucceeds(node, fixture.execution);
    assert(fixture.packetInput()->push(packet(MediaStreamKind::Video, 0x100000bc8LL)));
    processSucceeds(node, fixture.execution);
    MediaBufferRef firstOwner;
    const auto* first = popPacket(*fixture.output(), firstOwner);
    assert(first && first->packet()->duration == 3'000);

    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Video);
    assert(eof && fixture.packetInput()->push(eof.value()));
    processSucceeds(node, fixture.execution);
    MediaBufferRef lastOwner;
    const auto* last = popPacket(*fixture.output(), lastOwner);
    assert(last && last->packet()->duration == 3'000);
    processSucceeds(node, fixture.execution);
    MediaBufferRef terminal;
    assert(fixture.output()->tryPop(terminal) && terminal->isEof());

    BinderFixture noDelta(MediaStreamKind::Video);
    MediaRtpPacketClockBinderNode noDeltaNode(noDelta.binder);
    assert(noDelta.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(noDeltaNode, noDelta.execution);
    assert(noDelta.packetInput()->push(packet(MediaStreamKind::Video, 100)));
    processSucceeds(noDeltaNode, noDelta.execution);
    auto earlyEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Video);
    assert(earlyEof && noDelta.packetInput()->push(earlyEof.value()));
    assert(!noDeltaNode.process(noDelta.execution));
}

void outputWouldBlockRetriesWithoutLosingPacketOwnership()
{
    BinderFixture fixture(MediaStreamKind::Audio, 2, 1);
    MediaRtpPacketClockBinderNode node(fixture.binder);
    auto blocker = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    assert(blocker && fixture.output()->push(blocker.value()));
    assert(fixture.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(node, fixture.execution);
    auto original = packet(MediaStreamKind::Audio, 48'480, 480);
    const auto* originalPacket = dynamic_cast<const FFmpegPacketBuffer*>(original.get());
    const AVPacket* originalAddress = originalPacket ? originalPacket->packet() : nullptr;
    assert(originalAddress && fixture.packetInput()->push(original));
    auto blocked = node.process(fixture.execution);
    assert(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    MediaBufferRef removed;
    assert(fixture.output()->tryPop(removed) && removed == blocker.value());
    processSucceeds(node, fixture.execution);
    MediaBufferRef emittedOwner;
    const auto* emitted = popPacket(*fixture.output(), emittedOwner);
    assert(emitted && emitted->packet() == originalAddress);
}

void fanoutPublishesTheSameImmutableSnapshotBuffer()
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "clock.source");
    const auto fanout = graph.addNode(MediaNodeKind::RtpClockSnapshotFanout,
                                      "clock.fanout");
    graph.addOutputPort(source, "clock", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(fanout, "clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto queue = MediaGraphBuildSupport::blockingQueuePolicy(2);
    assert(graph.connect(source, "clock", fanout, "clock", "clock.in", queue));
    std::vector<MediaNodeId> sinks;
    for (const char* name : {"video", "audio", "startup"}) {
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, name);
        sinks.push_back(sink);
        graph.addOutputPort(fanout, name, MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(sink, "clock", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        assert(graph.connect(fanout, name, sink, "clock", name, queue));
    }
    MediaGraphExecutionContext execution;
    assert(execution.compile(graph));
    auto snapshot = makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot());
    assert(execution.findInputChannel(fanout, "clock")->push(snapshot));
    MediaRtpClockSnapshotFanoutNode node(fanout);
    processSucceeds(node, execution);
    for (const auto sink : sinks) {
        MediaBufferRef observed;
        assert(execution.findInputChannel(sink, "clock")->tryPop(observed));
        assert(observed == snapshot);
    }
}

} // namespace

int main()
{
    projectionUsesOneLockedEpochAndAcceptsExtendedWrapAndReordering();
    registeredNodeKindsHaveStableDiagnosticNames();
    validatorKeepsOneCommonEpochForAGroupGeneration();
    malformedOrUnavailableSnapshotsFailClosed();
    clockAndPacketMayArriveInEitherOrderWithMatchingGeneration();
    acquiringIsBoundedAndEvidenceAndGenerationChangesFailClosed();
    validatorMayRelockButBinderRejectsTheNewGroupGeneration();
    videoDurationUsesLookaheadAndOnlyExplicitTerminalReuse();
    outputWouldBlockRetriesWithoutLosingPacketOwnership();
    fanoutPublishesTheSameImmutableSnapshotBuffer();
    return 0;
}
