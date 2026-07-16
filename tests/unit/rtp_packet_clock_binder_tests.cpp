#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h"
#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupPolicy.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketTimestampAligner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

using namespace media::ffmpeg::graph;

static_assert(static_cast<std::uint8_t>(
                  MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime) == 0);

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

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(MediaRunningTime now) : m_now(now.nanoseconds()) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(m_now.load()));
    }

    void set(MediaRunningTime now) noexcept { m_now.store(now.nanoseconds()); }

private:
    std::atomic<std::int64_t> m_now;
};

MediaAvSyncPlan completeAvSyncPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "rtp-binder-tests";
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
    auto plan = MediaAvSyncPlanner::plan(request);
    assert(plan);
    auto finalized = std::move(plan).value();
    finalized.audioServo.commandLeadNs =
        MediaRunningTime::fromNanoseconds(1'500'000'000);
    finalized.audioServo.compensationWindowNs =
        MediaRunningTime::fromNanoseconds(2'000'000'000);
    finalized.audioServo.frequencyFilterTimeConstantNs =
        MediaRunningTime::fromNanoseconds(5'000'000'000);
    return finalized;
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
    auto registered = execution.registerAvSyncGroup(
        MediaAvSyncGroupKey("rtp-binder-group"), completeAvSyncPlan(), clock,
        std::make_shared<MediaSharedNtpEpoch>(std::move(epoch).value()),
        std::move(service).value());
    if (!registered) {
        std::fprintf(stderr, "sync group registration failed: %s\n",
                     registered.error().describe().c_str());
    }
    assert(registered);
}

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
    wrapped.value()->setTimeDescriptor(MediaTimeDescriptor{
        MediaRational{1, stream == MediaStreamKind::Video ? 90'000 : 48'000}});
    return std::move(wrapped).value();
}

void rawPacketTimestampsAlignToLockedSenderReportAnchor()
{
    MediaRtpPacketTimestampAligner aligner;
    const auto video = lockedSnapshot().locked->video;
    auto wrapped = aligner.align(video, 0x00000010U);
    auto reordered = aligner.align(video, 0xffffffe0U);
    assert(wrapped && wrapped.value() == 0x100000010ULL);
    assert(reordered && reordered.value() == 0xffffffe0ULL);

    assert(!aligner.align(video, video.rtpAnchor + 0x80000000U));
    auto overflow = video;
    overflow.rtpAnchor = std::numeric_limits<std::uint32_t>::max();
    overflow.extendedRtpAnchor = std::numeric_limits<std::int64_t>::max();
    assert(!aligner.align(overflow, 0));
    auto underflow = video;
    underflow.rtpAnchor = 0;
    underflow.extendedRtpAnchor = 0;
    assert(!aligner.align(underflow, std::numeric_limits<std::uint32_t>::max()));
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
            5'000'000'000LL, 5'000'000'000LL,
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime});
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

template <typename Node>
void processSucceeds(Node& node, MediaGraphExecutionContext& execution);

struct BinderFixture final {
    MediaGraph graph;
    MediaNodeId packetSource;
    MediaNodeId clockSource;
    MediaNodeId binder;
    MediaNodeId sink;
    MediaGraphExecutionContext execution;
    std::shared_ptr<TestMasterClock> masterClock;

    BinderFixture(MediaStreamKind stream, std::size_t acquiringCapacity = 2,
                  std::size_t outputCapacity = 4, bool includeSyncGroup = true)
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
        if (includeSyncGroup) {
            assert(graph.setNodeOption(binder, "rtp_clock_binder.sync_group",
                                       "rtp-binder-group"));
        }
        if (stream == MediaStreamKind::Video) {
            assert(graph.setNodeOption(binder, "rtp_clock_binder.duration_clock_rate",
                                       "90000"));
            assert(graph.setNodeOption(
                binder, "rtp_clock_binder.terminal_duration_policy",
                "repeat_last_observed_positive_delta"));
        }
        assert(execution.compile(graph));
        masterClock = std::make_shared<TestMasterClock>(
            MediaRunningTime::fromNanoseconds(0));
        registerSyncGroup(execution, masterClock);
    }

    MediaChannel* packetInput() { return execution.findInputChannel(binder, "packet"); }
    MediaChannel* clockInput() { return execution.findInputChannel(binder, "clock"); }
    MediaChannel* output() { return execution.findOutputChannel(binder, "packet"); }
};

void acquiringDeadlineUsesRegisteredMasterClock()
{
    BinderFixture expired(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode expiredNode(expired.binder);
    assert(expired.packetInput()->push(packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(expiredNode, expired.execution);
    auto waiting = expiredNode.process(expired.execution);
    assert(waiting && waiting.value().deadlineWait);
    assert(waiting.value().deadlineWait->syncGroup ==
           MediaAvSyncGroupKey("rtp-binder-group"));
    assert(waiting.value().deadlineWait->masterDeadline ==
           MediaRunningTime::fromNanoseconds(1'000'000'000));
    expired.masterClock->set(MediaRunningTime::fromNanoseconds(999'999'999));
    waiting = expiredNode.process(expired.execution);
    assert(waiting && waiting.value().deadlineWait);
    expired.masterClock->set(MediaRunningTime::fromNanoseconds(1'000'000'000));
    assert(!expiredNode.process(expired.execution));

    BinderFixture lockAtDeadline(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode lockAtDeadlineNode(lockAtDeadline.binder);
    assert(lockAtDeadline.packetInput()->push(
        packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(lockAtDeadlineNode, lockAtDeadline.execution);
    assert(lockAtDeadline.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    lockAtDeadline.masterClock->set(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    assert(!lockAtDeadlineNode.process(lockAtDeadline.execution));

    BinderFixture acquiringAtDeadline(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode acquiringAtDeadlineNode(
        acquiringAtDeadline.binder);
    assert(acquiringAtDeadline.packetInput()->push(
        packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(acquiringAtDeadlineNode, acquiringAtDeadline.execution);
    assert(acquiringAtDeadline.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(
            MediaRtpClockGroupSnapshot{
                MediaRtpClockGroupState::Acquiring, 0, std::nullopt})));
    acquiringAtDeadline.masterClock->set(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    assert(!acquiringAtDeadlineNode.process(acquiringAtDeadline.execution));

    BinderFixture locked(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode lockedNode(locked.binder);
    assert(locked.packetInput()->push(packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(lockedNode, locked.execution);
    assert(locked.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(lockedNode, locked.execution);
    locked.masterClock->set(MediaRunningTime::fromNanoseconds(2'000'000'000));
    processSucceeds(lockedNode, locked.execution);
    auto afterLock = lockedNode.process(locked.execution);
    assert(afterLock && !afterLock.value().deadlineWait);

    BinderFixture reset(MediaStreamKind::Audio);
    MediaRtpPacketClockBinderNode resetNode(reset.binder);
    assert(reset.packetInput()->push(packet(MediaStreamKind::Audio, 48'000, 480)));
    processSucceeds(resetNode, reset.execution);
    assert(resetNode.stop(reset.execution));
    auto afterStop = resetNode.process(reset.execution);
    assert(afterStop && !afterStop.value().deadlineWait);
    assert(reset.packetInput()->push(packet(MediaStreamKind::Audio, 48'480, 480)));
    processSucceeds(resetNode, reset.execution);
    resetNode.abort(reset.execution);
    auto afterAbort = resetNode.process(reset.execution);
    assert(afterAbort && !afterAbort.value().deadlineWait);

    BinderFixture missing(MediaStreamKind::Audio, 2, 4, false);
    MediaRtpPacketClockBinderNode missingNode(missing.binder);
    assert(!missingNode.process(missing.execution));
}

void validatorRequiresExplicitSupportedCommonEpochPolicy()
{
    MediaRtpClockGroupValidatorConfig config{
        3'000'000'000LL, 5'000'000'000LL, 50'000'000,
        5'000'000'000LL, 5'000'000'000LL,
        MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime};
    assert(MediaRtpClockGroupValidator::create(config));
    config.commonEpochPolicy = MediaRtpCommonEpochPolicy::Unknown;
    assert(!MediaRtpClockGroupValidator::create(config));
    config.commonEpochPolicy = static_cast<MediaRtpCommonEpochPolicy>(99);
    assert(!MediaRtpClockGroupValidator::create(config));
}

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
            5'000'000'000LL, 5'000'000'000LL,
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime});
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
    assert(fixture.packetInput()->push(packet(MediaStreamKind::Video, 0x10)));
    processSucceeds(node, fixture.execution);
    assert(fixture.packetInput()->push(packet(MediaStreamKind::Video, 0xbc8)));
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

void videoDurationClockRateRejectsMismatchedPacketTimeBase()
{
    BinderFixture fixture(MediaStreamKind::Video);
    MediaRtpPacketClockBinderNode node(fixture.binder);
    assert(fixture.clockInput()->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(lockedSnapshot())));
    processSucceeds(node, fixture.execution);
    auto mismatched = packet(MediaStreamKind::Video, 0x10);
    mismatched->setTimeDescriptor(MediaTimeDescriptor{MediaRational{1, 48'000}});
    assert(fixture.packetInput()->push(std::move(mismatched)));
    assert(!node.process(fixture.execution));
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
    rawPacketTimestampsAlignToLockedSenderReportAnchor();
    projectionUsesOneLockedEpochAndAcceptsExtendedWrapAndReordering();
    registeredNodeKindsHaveStableDiagnosticNames();
    validatorKeepsOneCommonEpochForAGroupGeneration();
    validatorRequiresExplicitSupportedCommonEpochPolicy();
    malformedOrUnavailableSnapshotsFailClosed();
    acquiringDeadlineUsesRegisteredMasterClock();
    clockAndPacketMayArriveInEitherOrderWithMatchingGeneration();
    acquiringIsBoundedAndEvidenceAndGenerationChangesFailClosed();
    validatorMayRelockButBinderRejectsTheNewGroupGeneration();
    videoDurationUsesLookaheadAndOnlyExplicitTerminalReuse();
    videoDurationClockRateRejectsMismatchedPacketTimeBase();
    outputWouldBlockRetriesWithoutLosingPacketOwnership();
    fanoutPublishesTheSameImmutableSnapshotBuffer();
    return 0;
}
