#include "common/TestAssert.h"

#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"
#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/HardwareFrameBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/time/MediaCanonicalTimeMapper.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

#include <chrono>
#include <limits>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionServiceTestAccess final {
    static ::media::Status activateInitial(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin origin)
    {
        return service->activateInitial(std::move(epoch), std::move(origin));
    }
};

} // namespace media::ffmpeg::graph

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr MediaRunningTime ns(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value);
}

class FixedMasterClock final : public MediaMasterClock {
public:
    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(ns(1'000));
    }
};

class ExtractorTestPurgeTarget final
    : public MediaAvGenerationPurgeTarget {
public:
    ::media::Status purge(
        const MediaAvGenerationPurge&) override
    {
        return ::media::Status::success();
    }
};

MediaAvSyncPlan extractorTestPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "canonical-lineage-extractor";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout =
        RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout =
        RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 8;
    request.avSyncStartup.maximumVideoUnitBytes = 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024;
    request.avSyncStartup.maximumGap = ns(1'000);
    auto plan = std::move(MediaAvSyncPlanner::plan(request)).value();
    plan.audioServo.commandLeadNs = ns(1'500'000'000);
    plan.audioServo.compensationWindowNs = ns(2'000'000'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ns(5'000'000'000);
    return plan;
}

bool registerActiveExtractorGroup(
    TestContext& ctx,
    MediaGraphExecutionContext& execution,
    MediaAvSyncGroupKey key,
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin origin)
{
    auto transition = MediaAvEpochTransitionService::create(
        MediaAvGenerationTransitionPlan{
            {{MediaAvGenerationParticipant::Scheduler, {"scheduler"}}},
            ns(1'000),
            ns(500)});
    EXPECT_TRUE(ctx, transition);
    if (!transition) return false;
    auto ntp = MediaSharedNtpEpoch::create(
        ns(0), std::chrono::nanoseconds(0));
    EXPECT_TRUE(ctx, ntp);
    if (!ntp) return false;
    auto clock = std::make_shared<FixedMasterClock>();
    EXPECT_TRUE(ctx, execution.registerAvSyncGroup(
                         key,
                         extractorTestPlan(),
                         clock,
                         std::make_shared<const MediaSharedNtpEpoch>(
                             std::move(ntp).value()),
                         transition.value()));
    auto participant = MediaAvGenerationParticipantGroup::create(
        {MediaAvGenerationParticipant::Scheduler, {"scheduler"}});
    EXPECT_TRUE(ctx, participant);
    if (!participant) return false;
    auto scheduler = std::make_shared<ExtractorTestPurgeTarget>();
    EXPECT_TRUE(ctx, participant.value().registerChild(
                         "scheduler", scheduler));
    EXPECT_TRUE(ctx, participant.value().seal());
    std::vector<MediaAvGenerationParticipantGroup> participants;
    participants.push_back(std::move(participant).value());
    auto coordinator = MediaAvReacquisitionCoordinator::create(
        transition.value(), clock, std::move(participants));
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return false;
    auto group = execution.findAvSyncGroup(key);
    EXPECT_TRUE(ctx, group != nullptr);
    if (!group) return false;
    EXPECT_TRUE(ctx, group->installReacquisitionCoordinator(
                         std::move(coordinator).value()));
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateInitial(
            transition.value(), std::move(epoch), std::move(origin)));
    return true;
}

MediaBufferRef packet(MediaStreamKind stream)
{
    auto value = ::media::ffmpeg::makePacket();
    av_new_packet(value.get(), 16);
    value->duration = 40;
    value->time_base = AVRational{1, 1'000'000'000};
    return FFmpegBufferFactory::wrapPacket(std::move(value), stream, std::nullopt).value();
}

MediaBufferRef timedPacket(MediaStreamKind stream,
                           std::optional<std::int64_t> presentation,
                           std::uint64_t generation = 7)
{
    auto value = ::media::ffmpeg::makePacket();
    av_new_packet(value.get(), 16);
    value->duration = 40;
    value->time_base = AVRational{1, 90'000};
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(value), stream,
        MediaPacketSourceTiming{presentation, presentation,
                                MediaSourceClockReadiness::Locked,
                                generation}).value();
    wrapped->setTimeDescriptor(
        MediaTimeDescriptor{MediaRational{1, 90'000}});
    return wrapped;
}

std::shared_ptr<const MediaCanonicalLineage> lineage(
    MediaScheduledStream stream,
    std::uint64_t sequence = 1)
{
    (void)stream;
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        ns(1'000), ns(900), ns(40),
        MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
        "validated-source", MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked, 7});
}

void testValidatedRtpAndTsEvidenceMapsExactly(TestContext& ctx)
{
    auto rtpMapper = MediaCanonicalTimeMapper::create({
        ns(5'000), ns(20'000), MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "rtp-video-ssrc-123", 7});
    EXPECT_TRUE(ctx, rtpMapper);
    if (rtpMapper) {
        auto mapped = rtpMapper.value().map(MediaCanonicalSourceTimestamp(
            ns(5'900), ns(5'800), ns(40), 7, "rtp-video-ssrc-123",
            MediaTimeMappingConfidence::Locked));
        EXPECT_TRUE(ctx, mapped);
        auto canonical = mapped ? createMediaCanonicalLineage(
            mapped.value(), MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
            MediaSourceAccessUnitSequence(1))
            : ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
                  ::media::ErrorInfo::invalidArgument("mapping failed"));
        EXPECT_TRUE(ctx, canonical);
        if (canonical) {
            EXPECT_EQ(ctx, canonical.value()->presentation, ns(20'900));
            EXPECT_EQ(ctx, canonical.value()->decode,
                      std::optional<MediaRunningTime>(ns(20'800)));
            EXPECT_EQ(ctx, canonical.value()->sourceIdentity,
                      std::string("rtp-video-ssrc-123"));
            EXPECT_EQ(ctx, canonical.value()->mappingConfidence,
                      MediaTimeMappingConfidence::Locked);
        }
    }

    auto tsMapper = MediaCanonicalTimeMapper::create({
        ns(27'000), ns(50'000), MediaAvSyncTopology::MpegTsToMpegTs,
        "mpegts-program-1-pid-256", 11});
    EXPECT_TRUE(ctx, tsMapper);
    if (tsMapper) {
        auto mapped = tsMapper.value().map(MediaCanonicalSourceTimestamp(
            ns(28'000), ns(27'500), ns(40), 11,
            "mpegts-program-1-pid-256", MediaTimeMappingConfidence::Locked));
        EXPECT_TRUE(ctx, mapped);
        auto canonical = mapped ? createMediaCanonicalLineage(
            mapped.value(), MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
            MediaSourceAccessUnitSequence(3))
            : ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
                  ::media::ErrorInfo::invalidArgument("mapping failed"));
        EXPECT_TRUE(ctx, canonical);
        if (canonical) {
            EXPECT_EQ(ctx, canonical.value()->presentation, ns(51'000));
            EXPECT_EQ(ctx, canonical.value()->decode,
                      std::optional<MediaRunningTime>(ns(50'500)));
            EXPECT_EQ(ctx, canonical.value()->generation,
                      static_cast<std::uint64_t>(11));
        }
    }
}

void testCanonicalInputRejectsMissingProtocolTime(TestContext& ctx)
{
    auto mapper = MediaCanonicalTimeMapper::create({
        ns(0), ns(0), MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "rtp-audio", 7});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;
    auto missing = MediaCanonicalInputNode::canonicalize(
        packet(MediaStreamKind::Audio),
        MediaPacketSourceTiming{std::nullopt, std::nullopt,
                                MediaSourceClockReadiness::Locked, 7},
        ns(20), MediaScheduledStream::Audio,
        MediaDecodeOrderMode::PresentationOrderNoReorder, "rtp-audio",
        MediaSourceAccessUnitSequence(1),
        MediaCanonicalAudioSampleInterval{0, 1, 48'000});
    EXPECT_FALSE(ctx, missing);
}

void testCanonicalAccessUnitRequiresExactAudioInterval(TestContext& ctx)
{
    auto audioPacket = packet(MediaStreamKind::Audio);
    auto videoPacket = packet(MediaStreamKind::Video);
    auto audioLineage = lineage(MediaScheduledStream::Audio);
    auto videoLineage = lineage(MediaScheduledStream::Video);

    EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
                          audioPacket, audioLineage, std::nullopt));
    EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
                          videoPacket, videoLineage,
                          MediaCanonicalAudioSampleInterval{0, 1'024, 48'000}));
    EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
                          audioPacket, audioLineage,
                          MediaCanonicalAudioSampleInterval{1'024, 1'024, 48'000}));
    EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
                          audioPacket, audioLineage,
                          MediaCanonicalAudioSampleInterval{0, 1'024, 0}));

    auto audio = MediaCanonicalAccessUnitBuffer::create(
        audioPacket, audioLineage,
        MediaCanonicalAudioSampleInterval{5'444, 6'468, 44'100});
    EXPECT_TRUE(ctx, audio);
    if (audio) {
        const auto* canonical =
            dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
                audio.value().get());
        EXPECT_TRUE(ctx, canonical != nullptr);
        if (canonical) {
            EXPECT_TRUE(ctx, canonical->audioSampleInterval().has_value());
            if (canonical->audioSampleInterval()) {
                EXPECT_EQ(
                    ctx, canonical->audioSampleInterval()->begin,
                    static_cast<std::int64_t>(5'444));
                EXPECT_EQ(
                    ctx, canonical->audioSampleInterval()->end,
                    static_cast<std::int64_t>(6'468));
                EXPECT_EQ(
                    ctx, canonical->audioSampleInterval()->sampleRate, 44'100);
            }
        }
    }

    auto video = MediaCanonicalAccessUnitBuffer::create(
        videoPacket, videoLineage, std::nullopt);
    EXPECT_TRUE(ctx, video);
    if (video) {
        const auto* canonical =
            dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
                video.value().get());
        EXPECT_TRUE(ctx, canonical != nullptr);
        if (canonical) {
            EXPECT_FALSE(ctx, canonical->audioSampleInterval().has_value());
        }
    }
}

void testLineageAndPayloadIdentitySurviveWrappers(TestContext& ctx)
{
    auto encoded = packet(MediaStreamKind::Video);
    auto immutable = lineage(MediaScheduledStream::Video);
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        encoded, immutable, std::nullopt);
    EXPECT_TRUE(ctx, canonical);
    if (!canonical) return;
    const auto* canonicalUnit = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
        canonical.value().get());
    EXPECT_TRUE(ctx, canonicalUnit != nullptr);
    if (!canonicalUnit) return;
    EXPECT_TRUE(ctx, canonicalUnit->media().get() == encoded.get());
    EXPECT_TRUE(ctx, canonicalUnit->lineage().get() == immutable.get());

    auto frame = ::media::ffmpeg::makeFrame();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    auto frameBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Video).value();
    MediaFormatDescriptor format;
    format.streamKind = MediaStreamKind::Video;
    format.video.size = {16, 16};
    format.video.pixelFormat = "yuv420p";
    MediaTimeDescriptor time;
    time.timeBase = {1, 90'000};
    time.frameRate = {30, 1};
    MediaHardwareDescriptor hardware;
    hardware.deviceKind = MediaHardwareDeviceKind::CUDA;
    hardware.frameKind = MediaHardwareFrameKind::Hardware;
    hardware.deviceName = "cuda-test";
    frameBuffer->setFormatDescriptor(format);
    frameBuffer->setTimeDescriptor(time);
    frameBuffer->setHardwareDescriptor(hardware);
    frameBuffer->setTimestamps(9'000, 8'000, 3'000);
    frameBuffer->setFlags(MediaBufferFlag::KeyFrame |
                          MediaBufferFlag::HardwareBacked);
    auto wrappedFrame = MediaCanonicalVideoFrameBuffer::create(
        frameBuffer, canonicalUnit->lineage());
    EXPECT_TRUE(ctx, wrappedFrame);
    if (wrappedFrame) {
        const auto* typed = dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(
            wrappedFrame.value().get());
        EXPECT_TRUE(ctx, typed != nullptr);
        if (typed) {
            EXPECT_TRUE(ctx, typed->media().get() == frameBuffer.get());
            EXPECT_TRUE(ctx, typed->lineage().get() == immutable.get());
            EXPECT_EQ(ctx, typed->formatDescriptor().video.size.width, 16);
            EXPECT_EQ(ctx, typed->formatDescriptor().video.pixelFormat,
                      std::string("yuv420p"));
            EXPECT_EQ(ctx, typed->timeDescriptor().timeBase.num, 1);
            EXPECT_EQ(ctx, typed->timeDescriptor().timeBase.den, 90'000);
            EXPECT_EQ(ctx, typed->timeDescriptor().frameRate.num, 30);
            EXPECT_EQ(ctx, typed->timeDescriptor().frameRate.den, 1);
            EXPECT_EQ(ctx, typed->hardwareDescriptor().deviceKind,
                      MediaHardwareDeviceKind::CUDA);
            EXPECT_EQ(ctx, typed->hardwareDescriptor().deviceName,
                      std::string("cuda-test"));
            EXPECT_EQ(ctx, typed->pts(), static_cast<MediaTimeValue>(9'000));
            EXPECT_EQ(ctx, typed->dts(), static_cast<MediaTimeValue>(8'000));
            EXPECT_EQ(ctx, typed->duration(), static_cast<MediaDuration>(3'000));
            EXPECT_TRUE(ctx, typed->isKeyFrame());
            EXPECT_TRUE(ctx, typed->isHardwareBacked());
        }
    }

    auto hardwareFrame = ::media::ffmpeg::makeFrame();
    hardwareFrame->format = AV_PIX_FMT_CUDA;
    hardwareFrame->width = 16;
    hardwareFrame->height = 16;
    auto hardwareFrameBuffer = makeMediaBufferRef<HardwareFrameBuffer>(
        std::move(hardwareFrame), hardware);
    hardwareFrameBuffer->setStreamKind(MediaStreamKind::Video);
    hardwareFrameBuffer->setPayloadKind(MediaPayloadKind::Frame);
    auto wrappedHardwareFrame = MediaCanonicalVideoFrameBuffer::create(
        hardwareFrameBuffer, canonicalUnit->lineage());
    EXPECT_TRUE(ctx, wrappedHardwareFrame);

    auto samples = ::media::ffmpeg::makeFrame();
    samples->format = AV_SAMPLE_FMT_FLTP;
    samples->sample_rate = 48'000;
    samples->nb_samples = 960;
    auto samplesBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(samples), MediaStreamKind::Audio).value();
    auto signedSamples = MediaCanonicalAudioSamplesBuffer::create(
        samplesBuffer, immutable, {-400, 560, 48'000});
    EXPECT_TRUE(ctx, signedSamples);
    if (signedSamples) {
        const auto* typed = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
            signedSamples.value().get());
        EXPECT_TRUE(ctx, typed != nullptr);
        if (typed) {
            EXPECT_EQ(ctx, typed->interval().begin,
                      static_cast<std::int64_t>(-400));
            EXPECT_EQ(ctx, typed->interval().end,
                      static_cast<std::int64_t>(560));
        }
    }
    auto overflowedSamples = MediaCanonicalAudioSamplesBuffer::create(
        samplesBuffer, immutable,
        {std::numeric_limits<std::int64_t>::min(),
         std::numeric_limits<std::int64_t>::max(), 48'000});
    EXPECT_FALSE(ctx, overflowedSamples);
    auto wrappedSamples = MediaCanonicalAudioSamplesBuffer::create(
        samplesBuffer, canonicalUnit->lineage(),
        MediaCanonicalAudioSampleInterval{0, 960, 48'000});
    EXPECT_TRUE(ctx, wrappedSamples);
    if (wrappedSamples) {
        const auto* typed = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
            wrappedSamples.value().get());
        EXPECT_TRUE(ctx, typed != nullptr);
        if (typed) {
            EXPECT_TRUE(ctx, typed->media().get() == samplesBuffer.get());
            EXPECT_TRUE(ctx, typed->lineage().get() == immutable.get());
        }
    }
}

void testReleaseContractRejectsWrongShape(TestContext& ctx)
{
    const MediaPlaybackEpoch epoch{ns(100), ns(200), 7};
    const MediaAudioPlaybackOrigin origin{7, ns(100), ns(200), 0, 48'000};
    std::vector<MediaAvReleasedUnit> video{{packet(MediaStreamKind::Video), 0}};
    std::vector<MediaAvReleasedUnit> audio{{packet(MediaStreamKind::Audio), 0}};
    auto initial = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"), MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch, origin, video, audio, std::nullopt);
    EXPECT_TRUE(ctx, initial);
    auto missingAudio = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"), MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch, origin, video, {}, std::nullopt);
    EXPECT_FALSE(ctx, missingAudio);
    auto activeVideo = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"), MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch, origin, video, {}, std::nullopt);
    EXPECT_TRUE(ctx, activeVideo);
    auto emptyActive = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"), MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch, origin, {}, {}, std::nullopt);
    EXPECT_FALSE(ctx, emptyActive);
    auto unknown = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"),
        static_cast<MediaAvStartupReleaseKind>(255), epoch, origin, video,
        audio, std::nullopt);
    EXPECT_FALSE(ctx, unknown);
    EXPECT_FALSE(ctx, MediaAvStartupReleaseBuffer::validateReleaseKind(
                          static_cast<MediaAvStartupReleaseKind>(255)));
}

void testGenerationStateRejectsDuplicateAndGroupMismatch(TestContext& ctx)
{
    MediaAvStartupGenerationState state(MediaAvSyncGroupKey("group-a"));
    MediaAvStartupAccessUnit unit{
        MediaAvStartupStream::Video, "validated-source", 9, 16, ns(1'000),
        ns(40), MediaSourceClockReadiness::Locked, 7, true, std::nullopt};
    EXPECT_TRUE(ctx, state.store(MediaAvSyncGroupKey("group-a"), unit,
                                 packet(MediaStreamKind::Video)));
    EXPECT_FALSE(ctx, state.store(MediaAvSyncGroupKey("group-a"), unit,
                                  packet(MediaStreamKind::Video)));
    unit.sequence = 10;
    EXPECT_FALSE(ctx, state.store(MediaAvSyncGroupKey("group-b"), unit,
                                  packet(MediaStreamKind::Video)));
}

void testGenerationStateRetainsHistoryAndPurgesOnlyByProtocol(TestContext& ctx)
{
    auto state = std::make_shared<MediaAvStartupGenerationState>(
        MediaAvSyncGroupKey("group-a"));
    MediaAvStartupAccessUnit unit{
        MediaAvStartupStream::Video, "validated-source", 9, 16, ns(1'000),
        ns(40), MediaSourceClockReadiness::Locked, 7, true, std::nullopt};
    EXPECT_TRUE(ctx, state->store(MediaAvSyncGroupKey("group-a"), unit,
                                  packet(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, state->take({MediaAvStartupStream::Video, 7, 9}));
    EXPECT_FALSE(ctx, state->store(MediaAvSyncGroupKey("group-a"), unit,
                                   packet(MediaStreamKind::Video)));
    unit.sequence = 10;
    EXPECT_TRUE(ctx, state->store(MediaAvSyncGroupKey("group-a"), unit,
                                  packet(MediaStreamKind::Video)));
    state->erase({MediaAvStartupStream::Video, 7, 10});
    EXPECT_FALSE(ctx, state->store(MediaAvSyncGroupKey("group-a"), unit,
                                   packet(MediaStreamKind::Video)));

    auto malformedFuture = unit;
    malformedFuture.generation = 8;
    malformedFuture.sequence = 11;
    EXPECT_FALSE(ctx, state->store(MediaAvSyncGroupKey("group-a"), malformedFuture,
                                   packet(MediaStreamKind::Video)));
    unit.sequence = 12;
    EXPECT_TRUE(ctx, state->store(MediaAvSyncGroupKey("group-a"), unit,
                                  packet(MediaStreamKind::Video)));

    MediaAvGenerationParticipantPlan plan{
        MediaAvGenerationParticipant::CanonicalLineage,
        {std::string(MediaAvStartupGenerationState::plannedIdentity())}};
    auto group = MediaAvGenerationParticipantGroup::create(std::move(plan));
    EXPECT_TRUE(ctx, group);
    if (!group) return;
    EXPECT_TRUE(ctx, group.value().registerChild(
                         std::string(MediaAvStartupGenerationState::plannedIdentity()),
                         state));
    EXPECT_TRUE(ctx, group.value().seal());
    auto acknowledgement = group.value().purgeAll({7, 8, 1});
    EXPECT_TRUE(ctx, acknowledgement);
    if (acknowledgement) {
        EXPECT_EQ(ctx, acknowledgement.value().participant,
                  MediaAvGenerationParticipant::CanonicalLineage);
        EXPECT_EQ(ctx, acknowledgement.value().transitionSequence,
                  static_cast<std::uint64_t>(1));
        EXPECT_TRUE(ctx, acknowledgement.value().status);
    }
    malformedFuture.sequence = 9;
    EXPECT_TRUE(ctx, state->store(MediaAvSyncGroupKey("group-a"), malformedFuture,
                                  packet(MediaStreamKind::Video)));
}

void setCanonicalInputOptions(MediaGraph& graph, MediaNodeId node)
{
    graph.setNodeOption(node, "canonical_input.stream", "video");
    graph.setNodeOption(node, "canonical_input.source_identity", "rtp-video");
    graph.setNodeOption(node, "canonical_input.duration_source", "packet");
    graph.setNodeOption(node, "canonical_input.decode_order", "reordered");
}

struct CanonicalInputHarness final {
    MediaGraph graph;
    MediaNodeId node;
    MediaGraphExecutionContext execution;
    std::unique_ptr<MediaCanonicalInputNode> runtime;

    bool initialize(TestContext& ctx, std::size_t outputCapacity)
    {
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        node = graph.addNode(MediaNodeKind::CanonicalInput, "canonical");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
        const auto sourceOutput = graph.addOutputPort(
            source, "out", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        const auto canonicalInput = graph.addInputPort(
            node, "in", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        const MediaTimeDescriptor packetTime{MediaRational{1, 90'000}};
        graph.setPortTimeDescriptor(sourceOutput, packetTime);
        graph.setPortTimeDescriptor(canonicalInput, packetTime);
        graph.addOutputPort(node, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.connect(source, "out", node, "in", "input",
                      MediaGraphBuildSupport::blockingQueuePolicy(4));
        graph.connect(node, "out", sink, "in", "output",
                      MediaGraphBuildSupport::blockingQueuePolicy(outputCapacity));
        setCanonicalInputOptions(graph, node);
        EXPECT_TRUE(ctx, execution.compile(graph));
        runtime = std::make_unique<MediaCanonicalInputNode>(node);
        EXPECT_TRUE(ctx, runtime->start(execution));
        return execution.findInputChannel(node, "in") &&
               execution.findOutputChannel(node, "out");
    }
};

void testCanonicalSequenceAdvancesOnceAcrossRetainedTransfer(TestContext& ctx)
{
    CanonicalInputHarness harness;
    if (!harness.initialize(ctx, 1)) return;
    MediaChannel* input = harness.execution.findInputChannel(harness.node, "in");
    MediaChannel* output = harness.execution.findOutputChannel(harness.node, "out");
    EXPECT_TRUE(ctx, output->push(makeMediaBufferRef<MediaAvStartupClockBuffer>(ns(1))));
    EXPECT_TRUE(ctx, input->push(timedPacket(MediaStreamKind::Video, 1'100)));
    auto retained = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, retained);
    if (retained) EXPECT_EQ(ctx, retained.value().state, MediaNodeProcessState::Waiting);
    MediaBufferRef blocker;
    EXPECT_TRUE(ctx, output->tryPop(blocker));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    MediaBufferRef first;
    EXPECT_TRUE(ctx, output->tryPop(first));
    const auto* firstEnvelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(first.get());
    EXPECT_TRUE(ctx, firstEnvelope != nullptr);
    if (firstEnvelope) EXPECT_EQ(ctx, firstEnvelope->unit().sequence,
                                 static_cast<std::uint64_t>(1));
    EXPECT_TRUE(ctx, input->push(timedPacket(MediaStreamKind::Video, 1'200)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    MediaBufferRef second;
    EXPECT_TRUE(ctx, output->tryPop(second));
    const auto* secondEnvelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(second.get());
    EXPECT_TRUE(ctx, secondEnvelope != nullptr);
    if (secondEnvelope) EXPECT_EQ(ctx, secondEnvelope->unit().sequence,
                                  static_cast<std::uint64_t>(2));
}

void testFailedMappingDoesNotConsumeCanonicalSequence(TestContext& ctx)
{
    CanonicalInputHarness harness;
    if (!harness.initialize(ctx, 4)) return;
    auto* input = harness.execution.findInputChannel(harness.node, "in");
    auto* output = harness.execution.findOutputChannel(harness.node, "out");
    EXPECT_TRUE(ctx, input->push(timedPacket(MediaStreamKind::Video, std::nullopt)));
    EXPECT_FALSE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, input->push(timedPacket(MediaStreamKind::Video, 1'100)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    MediaBufferRef emitted;
    EXPECT_TRUE(ctx, output->tryPop(emitted));
    const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(emitted.get());
    EXPECT_TRUE(ctx, envelope != nullptr);
    if (envelope) EXPECT_EQ(ctx, envelope->unit().sequence,
                            static_cast<std::uint64_t>(1));
}

void testExtractorPreflightsCompoundReleaseWithoutPartialCommit(TestContext& ctx)
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "release-source");
    const auto extractor = graph.addNode(MediaNodeKind::AvBoundReleaseExtractor,
                                         "release-extractor");
    const auto videoSink = graph.addNode(MediaNodeKind::DebugDump, "video-sink");
    const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio-sink");
    graph.addOutputPort(source, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(extractor, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(extractor, "video", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(extractor, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(videoSink, "in", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(audioSink, "in", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.connect(source, "out", extractor, "in", "release",
                  MediaGraphBuildSupport::blockingQueuePolicy(2));
    graph.connect(extractor, "video", videoSink, "in", "video",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(1));
    graph.connect(extractor, "audio", audioSink, "in", "audio",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(1));
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaAvBoundReleaseExtractorNode node(
        extractor, MediaAvSyncGroupKey("group-a"));
    EXPECT_TRUE(ctx, node.start(execution));

    MediaChannel* video = execution.findOutputChannel(extractor, "video");
    MediaChannel* audio = execution.findOutputChannel(extractor, "audio");
    EXPECT_TRUE(ctx, video != nullptr && audio != nullptr);
    if (!video || !audio) return;
    EXPECT_TRUE(ctx, audio->push(packet(MediaStreamKind::Audio)));
    auto canonicalAudio = MediaCanonicalAccessUnitBuffer::create(
        packet(MediaStreamKind::Audio), lineage(MediaScheduledStream::Audio, 2),
        MediaCanonicalAudioSampleInterval{0, 480, 48'000});
    EXPECT_TRUE(ctx, canonicalAudio);
    if (!canonicalAudio) return;
    const MediaPlaybackEpoch epoch{ns(100), ns(200), 7};
    const MediaAudioPlaybackOrigin origin{7, ns(100), ns(200), 0, 48'000};
    if (!registerActiveExtractorGroup(
            ctx,
            execution,
            MediaAvSyncGroupKey("group-a"),
            epoch,
            origin)) {
        return;
    }
    auto release = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"), MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch, origin, {{packet(MediaStreamKind::Video), 0}},
        {{canonicalAudio.value(), 0}}, std::nullopt);
    EXPECT_TRUE(ctx, release);
    if (!release) return;
    EXPECT_TRUE(ctx, execution.findInputChannel(extractor, "in")->push(
                         release.value()));
    auto blocked = node.process(execution);
    EXPECT_TRUE(ctx, blocked);
    if (blocked) EXPECT_EQ(ctx, blocked.value().state, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, video->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audio->size(), static_cast<std::size_t>(1));
    MediaBufferRef blocker;
    EXPECT_TRUE(ctx, audio->tryPop(blocker));
    auto committed = node.process(execution);
    EXPECT_TRUE(ctx, committed);
    EXPECT_EQ(ctx, video->size(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, audio->size(), static_cast<std::size_t>(1));
}

void testExtractorPreservesAudioTrimAndIdentity(TestContext& ctx)
{
    const MediaAudioPlaybackOrigin invalidOrigin{7, ns(100), ns(200), 0, 48'000};
    EXPECT_FALSE(ctx, MediaAvReleasedAudioBuffer::create(
                          packet(MediaStreamKind::Audio), 321, invalidOrigin));
    auto immutable = lineage(MediaScheduledStream::Audio);
    auto encoded = packet(MediaStreamKind::Audio);
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        encoded, immutable,
        MediaCanonicalAudioSampleInterval{0, 480, 48'000});
    EXPECT_TRUE(ctx, canonical);
    if (!canonical) return;
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto extractor = graph.addNode(MediaNodeKind::AvBoundReleaseExtractor, "extractor");
    const auto videoSink = graph.addNode(MediaNodeKind::DebugDump, "video");
    const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio");
    graph.addOutputPort(source, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(extractor, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(extractor, "video", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(extractor, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(videoSink, "in", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(audioSink, "in", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.connect(source, "out", extractor, "in", "release",
                  MediaGraphBuildSupport::blockingQueuePolicy(2));
    graph.connect(extractor, "video", videoSink, "in", "video",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(2));
    graph.connect(extractor, "audio", audioSink, "in", "audio",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(2));
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaAvBoundReleaseExtractorNode node(
        extractor, MediaAvSyncGroupKey("group-a"));
    EXPECT_TRUE(ctx, node.start(execution));
    const MediaPlaybackEpoch epoch{ns(100), ns(200), 7};
    const MediaAudioPlaybackOrigin origin{7, ns(100), ns(200), 0, 48'000};
    if (!registerActiveExtractorGroup(
            ctx,
            execution,
            MediaAvSyncGroupKey("group-a"),
            epoch,
            origin)) {
        return;
    }
    auto release = MediaAvStartupReleaseBuffer::create(
        MediaAvSyncGroupKey("group-a"),
        MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        epoch, origin, {}, {{canonical.value(), 321}}, std::nullopt);
    EXPECT_TRUE(ctx, release);
    if (!release) return;
    EXPECT_TRUE(ctx, execution.findInputChannel(extractor, "in")->push(release.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef output;
    EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "audio")->tryPop(output));
    const auto* typed = dynamic_cast<const MediaAvReleasedAudioBuffer*>(output.get());
    EXPECT_TRUE(ctx, typed != nullptr);
    if (typed) {
        EXPECT_EQ(ctx, typed->trimLeadingSamples(), static_cast<std::uint32_t>(321));
        EXPECT_TRUE(ctx, typed->audioOrigin() == origin);
        EXPECT_TRUE(ctx, typed->media().get() == canonical.value().get());
        const auto* outputCanonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
            typed->media().get());
        EXPECT_TRUE(ctx, outputCanonical != nullptr);
        if (outputCanonical) {
            EXPECT_TRUE(ctx, outputCanonical->media().get() == encoded.get());
            EXPECT_TRUE(ctx, outputCanonical->lineage().get() == immutable.get());
        }
    }
}

void testExtractorAtomicallyFansOutExactEofReference(TestContext& ctx)
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
    const auto extractor = graph.addNode(
        MediaNodeKind::AvBoundReleaseExtractor, "extractor");
    const auto videoSink = graph.addNode(MediaNodeKind::DebugDump, "video");
    const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio");
    graph.addOutputPort(source, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(extractor, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(extractor, "video", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(extractor, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(videoSink, "in", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(audioSink, "in", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.connect(source, "out", extractor, "in", "release",
                  MediaGraphBuildSupport::blockingQueuePolicy(2));
    graph.connect(extractor, "video", videoSink, "in", "video",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(1));
    graph.connect(extractor, "audio", audioSink, "in", "audio",
                  MediaGraphBuildSupport::atomicPreparedQueuePolicy(1));
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaAvBoundReleaseExtractorNode node(
        extractor, MediaAvSyncGroupKey("group-a"));
    EXPECT_TRUE(ctx, node.start(execution));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    auto blocker = packet(MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "audio")->push(blocker));
    EXPECT_TRUE(ctx, execution.findInputChannel(extractor, "in")->push(eof.value()));
    const auto blocked = node.process(execution);
    EXPECT_TRUE(ctx, blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, execution.findOutputChannel(extractor, "video")->size(),
              static_cast<std::size_t>(0));
    MediaBufferRef discarded;
    EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "audio")->tryPop(discarded));
    const auto committed = node.process(execution);
    EXPECT_TRUE(ctx, committed && committed.value().state ==
                                      MediaNodeProcessState::Finished);
    MediaBufferRef video;
    MediaBufferRef audio;
    EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "video")->tryPop(video));
    EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "audio")->tryPop(audio));
    EXPECT_TRUE(ctx, video == eof.value() && audio == eof.value());
}

void testExtractorHandlesTypedControlsAndRequiredInputTermination(TestContext& ctx)
{
    const auto verify = [&](std::optional<MediaControlBufferKind> kind,
                            bool abortInput) {
        MediaGraph graph;
        const auto source = graph.addNode(MediaNodeKind::DebugDump, "source");
        const auto extractor = graph.addNode(
            MediaNodeKind::AvBoundReleaseExtractor, "extractor");
        const auto videoSink = graph.addNode(MediaNodeKind::DebugDump, "video");
        const auto audioSink = graph.addNode(MediaNodeKind::DebugDump, "audio");
        graph.addOutputPort(source, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(extractor, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(extractor, "video", MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(extractor, "audio", MediaStreamKind::Audio,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(videoSink, "in", MediaStreamKind::Video,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        graph.addInputPort(audioSink, "in", MediaStreamKind::Audio,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
        const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
        const auto atomicPolicy =
            MediaGraphBuildSupport::atomicPreparedQueuePolicy(1);
        graph.connect(source, "out", extractor, "in", "release", policy);
        graph.connect(
            extractor, "video", videoSink, "in", "video", atomicPolicy);
        graph.connect(
            extractor, "audio", audioSink, "in", "audio", atomicPolicy);
        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));
        MediaAvBoundReleaseExtractorNode node(
            extractor, MediaAvSyncGroupKey("group-a"));
        EXPECT_TRUE(ctx, node.start(execution));
        MediaBufferRef control;
        if (kind) {
            control = makeMediaBufferRef<MediaControlBuffer>(*kind);
            EXPECT_TRUE(ctx, execution.findInputChannel(extractor, "in")->push(
                                 control));
        } else if (abortInput) {
            execution.findInputChannel(extractor, "in")->abort();
        } else {
            execution.findInputChannel(extractor, "in")->close();
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
        MediaBufferRef video;
        MediaBufferRef audio;
        EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "video")->tryPop(video));
        EXPECT_TRUE(ctx, execution.findOutputChannel(extractor, "audio")->tryPop(audio));
        EXPECT_TRUE(ctx, video == control && audio == control);
    };
    verify(MediaControlBufferKind::Flush, false);
    verify(MediaControlBufferKind::Abort, false);
    verify(std::nullopt, false);
    verify(std::nullopt, true);
}

} // namespace

int main()
{
    TestContext ctx;
    testValidatedRtpAndTsEvidenceMapsExactly(ctx);
    testCanonicalInputRejectsMissingProtocolTime(ctx);
    testCanonicalAccessUnitRequiresExactAudioInterval(ctx);
    testLineageAndPayloadIdentitySurviveWrappers(ctx);
    testReleaseContractRejectsWrongShape(ctx);
    testGenerationStateRejectsDuplicateAndGroupMismatch(ctx);
    testGenerationStateRetainsHistoryAndPurgesOnlyByProtocol(ctx);
    testCanonicalSequenceAdvancesOnceAcrossRetainedTransfer(ctx);
    testFailedMappingDoesNotConsumeCanonicalSequence(ctx);
    testExtractorPreflightsCompoundReleaseWithoutPartialCommit(ctx);
    testExtractorPreservesAudioTrimAndIdentity(ctx);
    testExtractorAtomicallyFansOutExactEofReference(ctx);
    testExtractorHandlesTypedControlsAndRequiredInputTermination(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
