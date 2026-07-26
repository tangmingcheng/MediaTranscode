#include "unit/fixtures/ScheduledRtpOutputNodeTestSupport.h"

#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

extern "C" {
#include <libavutil/mem.h>
}

#include <memory>
#include <future>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionServiceTestAccess final {
    static bool outputPermitMutexLocked(
        const std::shared_ptr<MediaAvEpochTransitionService>& service)
    {
        if (service->m_mutex.try_lock()) {
            service->m_mutex.unlock();
            return false;
        }
        return true;
    }

    static ::media::Status activateInitial(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        const MediaPlaybackEpoch& epoch)
    {
        return service->activateInitial(
            epoch,
            MediaAudioPlaybackOrigin{
                epoch.generation, epoch.sourceStart, epoch.masterRelease,
                0, 48'000});
    }

    static ::media::Status activateNext(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        std::uint64_t transitionSequence,
        const MediaPlaybackEpoch& epoch)
    {
        return service->activateNextAfter(
            transitionSequence, epoch,
            MediaAudioPlaybackOrigin{
                epoch.generation, epoch.sourceStart, epoch.masterRelease,
                0, 48'000});
    }
};

struct MediaScheduledRtpSenderNodeTestAccess final {
    static bool generationSessionCleared(
        const MediaScheduledRtpSenderNode& node) noexcept
    {
        return !node.m_sessionState->sender &&
            !node.m_sessionState->epoch &&
            !node.m_sessionState->activation &&
            !node.m_sessionState->description;
    }
};

} // namespace media::ffmpeg::graph

namespace media_transcode::test::scheduled_rtp_output {

using namespace media::ffmpeg::graph;
using namespace rtp_udp;

namespace {

void testPermitCloseDropsOldGenerationAndReusesTransport(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto runtimePlan = std::move(*outer.value().avSyncRuntime);
    auto transitionPlan = MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
        milliseconds(1'000), milliseconds(1'000));
    auto transition = MediaAvEpochTransitionService::create(transitionPlan);
    auto sharedNtp = MediaSharedNtpEpoch::create(
        milliseconds(0), std::chrono::seconds(1'700'000'000));
    auto clock = std::make_shared<TestMasterClock>(milliseconds(0));
    EXPECT_TRUE(ctx, transition && sharedNtp);
    if (!transition || !sharedNtp) return;
    auto group = MediaAvSyncGroupRuntime::create(
        runtimePlan.groupKey, runtimePlan.synchronization, clock,
        std::make_shared<const MediaSharedNtpEpoch>(
            std::move(sharedNtp).value()),
        transition.value());
    EXPECT_TRUE(ctx, group);
    if (!group) return;
    const MediaPlaybackEpoch firstEpoch{
        milliseconds(0), milliseconds(0), 1};
    EXPECT_TRUE(ctx, MediaAvEpochTransitionServiceTestAccess::activateInitial(
                         transition.value(), firstEpoch));
    auto sender = senderCase(
        ctx, runtimePlan, group.value(), MediaScheduledStream::Video,
        false, 40'000);
    if (!sender ||
        !pushActivationAndCodec(
            ctx, *sender, runtimePlan.groupKey, true)) {
        return;
    }
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    const int initialOpenCalls = sender->rtp->openCalls;

    auto inFlight = scheduledUnit(
        MediaScheduledStream::Video, sender->senderLead, 10, 1);
    EXPECT_TRUE(ctx, inFlight);
    if (!inFlight) return;
    EXPECT_TRUE(ctx, sender->graph.execution.findInputChannel(
                         sender->graph.sender, "scheduled")
                         ->push(std::move(inFlight).value()));
    {
        std::lock_guard lock(sender->rtp->blockMutex);
        sender->rtp->releaseSend = false;
        sender->rtp->sendEntered = false;
    }
    auto inFlightCommit = std::async(
        std::launch::async,
        [&sender] {
            return sender->node->process(sender->graph.execution);
        });
    {
        std::unique_lock lock(sender->rtp->blockMutex);
        sender->rtp->blockCondition.wait(
            lock, [&sender] { return sender->rtp->sendEntered; });
    }
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::outputPermitMutexLocked(
            transition.value()));
    std::promise<void> transitionStarted;
    auto transitionCommit = std::async(
        std::launch::async,
        [&transition, &transitionStarted] {
            transitionStarted.set_value();
            return transition.value()->beginReacquisition(1, 2);
        });
    transitionStarted.get_future().wait();
    EXPECT_EQ(
        ctx,
        transitionCommit.wait_for(std::chrono::milliseconds(0)),
        std::future_status::timeout);
    releasePort(sender->rtp);
    EXPECT_TRUE(ctx, inFlightCommit.get());
    auto purge = transitionCommit.get();
    EXPECT_TRUE(ctx, purge);
    if (!purge) return;
    EXPECT_TRUE(ctx, sender->node->generationPurgeTarget()->purge(
                         purge.value()));
    EXPECT_TRUE(ctx,
                MediaScheduledRtpSenderNodeTestAccess::
                    generationSessionCleared(*sender->node));
    EXPECT_EQ(ctx, sender->rtp->sendCalls, 1);

    auto closedGeneration = scheduledUnit(
        MediaScheduledStream::Video, sender->senderLead, 11, 1);
    EXPECT_TRUE(ctx, closedGeneration);
    if (!closedGeneration) return;
    EXPECT_TRUE(ctx, sender->graph.execution.findInputChannel(
                         sender->graph.sender, "scheduled")
                         ->push(std::move(closedGeneration).value()));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    EXPECT_EQ(ctx, sender->rtp->sendCalls, 1);

    for (const auto& participant :
         transition.value()->transitionPlan().participants) {
        EXPECT_TRUE(ctx, transition.value()->acknowledge(
                             MediaAvGenerationAcknowledgement{
                                 participant.participant,
                                 purge.value().transitionSequence,
                                 ::media::Status::success()}));
    }
    const MediaPlaybackEpoch secondEpoch{
        milliseconds(0), milliseconds(1'000), 2};
    EXPECT_TRUE(ctx, MediaAvEpochTransitionServiceTestAccess::activateNext(
                         transition.value(),
                         purge.value().transitionSequence, secondEpoch));
    auto activation = MediaPlaybackEpochActivatedBuffer::create(
        runtimePlan.groupKey, secondEpoch,
        {2, milliseconds(0), milliseconds(1'000), 0, 48'000},
        purge.value().transitionSequence);
    EXPECT_TRUE(ctx, activation);
    if (!activation) return;
    EXPECT_TRUE(ctx, sender->graph.execution.findInputChannel(
                         sender->graph.sender, "epoch")
                         ->push(std::move(activation).value()));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    auto current = scheduledUnit(
        MediaScheduledStream::Video, sender->senderLead, 12, 2);
    EXPECT_TRUE(ctx, current);
    if (!current) return;
    EXPECT_TRUE(ctx, sender->graph.execution.findInputChannel(
                         sender->graph.sender, "scheduled")
                         ->push(std::move(current).value()));
    EXPECT_TRUE(ctx, sender->node->process(sender->graph.execution));
    EXPECT_EQ(ctx, sender->rtp->openCalls, initialOpenCalls);
    EXPECT_EQ(ctx, sender->rtp->sendCalls, 2);
    EXPECT_TRUE(ctx, sender->node->stop(sender->graph.execution));
}

void testActivationOpensExactPlannedSenders(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto runtimePlan = std::move(*outer.value().avSyncRuntime);
    auto active = activeGroup(ctx, runtimePlan);
    if (!active.group) return;
    auto& separate = std::get<MediaSeparateRtpOutputRuntimePlan>(
        runtimePlan.protocolOutput);
    auto graph = senderGraph(ctx, MediaScheduledStream::Video);
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    auto rtpLocal = MediaUdpDatagramEndpoint::create(
        separate.video.transport.addressFamily(),
        separate.video.transport.localNumericAddress(), 41'000);
    auto rtcpLocal = MediaUdpDatagramEndpoint::create(
        separate.video.transport.addressFamily(),
        separate.video.transport.localNumericAddress(), 41'002);
    EXPECT_TRUE(ctx, rtpLocal && rtcpLocal);
    if (!rtpLocal || !rtcpLocal) return;
    rtp->scriptedBoundEndpoint = std::move(rtpLocal).value();
    rtcp->scriptedBoundEndpoint = std::move(rtcpLocal).value();
    releasePort(rtp);
    releasePort(rtcp);
    auto packetizer = std::make_shared<PacketizerState>();
    const MediaRunningTime videoLead = separate.video.senderLead;
    const MediaRunningTime videoReportInterval =
        separate.video.senderReportInterval;
    const std::uint32_t videoSsrc = separate.video.ssrc;
    const std::uint16_t videoRemotePort =
        separate.video.transport.remoteRtpEndpoint().port();
    auto created = MediaScheduledRtpSenderNode::create(
        graph.sender, runtimePlan.groupKey, std::move(separate.video),
        separate.sdp,
        MediaScheduledRtpSenderNodeDependencies{
            active.group,
            std::make_unique<FakeSenderPortFactory>(rtp, rtcp),
            std::make_unique<FakePacketizerFactory>(packetizer)});
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto node = std::move(created).value();
    EXPECT_TRUE(ctx, node->start(graph.execution));
    auto activation = MediaPlaybackEpochActivatedBuffer::create(
        runtimePlan.groupKey,
        {milliseconds(0), milliseconds(0), 1},
        {1, milliseconds(0), milliseconds(0), 0, 48'000},
        std::nullopt);
    EXPECT_TRUE(ctx, activation);
    auto codec = makeMediaBufferRef<FFmpegCodecContextBuffer>(
        codecContext(MediaScheduledStream::Video));
    EXPECT_TRUE(ctx, codec != nullptr);
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(graph.sender, "epoch")
                         ->push(std::move(activation).value()));
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(graph.sender, "codec")
                         ->push(codec));
    EXPECT_TRUE(ctx, node->process(graph.execution));
    auto opened = node->process(graph.execution);
    EXPECT_TRUE(ctx, opened);
    EXPECT_EQ(ctx, rtp->openCalls, 1);
    EXPECT_EQ(ctx, rtcp->openCalls, 1);
    EXPECT_EQ(ctx, packetizer->createCalls, 1);
    EXPECT_EQ(ctx, packetizer->openCalls, 1);
    EXPECT_EQ(ctx, packetizer->stream, MediaStreamKind::Video);
    EXPECT_EQ(ctx, packetizer->mode,
              MediaScheduledRtpPacketizationMode::H264AnnexB);
    EXPECT_EQ(ctx, packetizer->payloadType, 96);
    MediaBufferRef videoDescription;
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(
                         graph.descriptionSink, "description")
                         ->tryPop(videoDescription));
    const auto* typedVideo =
        dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
            videoDescription.get());
    EXPECT_TRUE(ctx, typedVideo != nullptr);
    if (typedVideo) {
        EXPECT_EQ(ctx, typedVideo->stream(), MediaScheduledStream::Video);
        EXPECT_EQ(ctx, typedVideo->generation(), std::uint64_t{1});
        EXPECT_EQ(ctx, typedVideo->session().cname(), separate.sdp.cname);
    }

    auto audioGraph = senderGraph(ctx, MediaScheduledStream::Audio);
    auto audioRtp = std::make_shared<FakePortState>();
    auto audioRtcp = std::make_shared<FakePortState>();
    auto audioRtpLocal = MediaUdpDatagramEndpoint::create(
        separate.audio.transport.addressFamily(),
        separate.audio.transport.localNumericAddress(), 42'000);
    auto audioRtcpLocal = MediaUdpDatagramEndpoint::create(
        separate.audio.transport.addressFamily(),
        separate.audio.transport.localNumericAddress(), 42'002);
    EXPECT_TRUE(ctx, audioRtpLocal && audioRtcpLocal);
    if (!audioRtpLocal || !audioRtcpLocal) return;
    audioRtp->scriptedBoundEndpoint = std::move(audioRtpLocal).value();
    audioRtcp->scriptedBoundEndpoint = std::move(audioRtcpLocal).value();
    releasePort(audioRtp);
    releasePort(audioRtcp);
    auto audioPacketizer = std::make_shared<PacketizerState>();
    const MediaRunningTime audioLead = separate.audio.senderLead;
    const MediaRunningTime audioReportInterval =
        separate.audio.senderReportInterval;
    const std::uint32_t audioSsrc = separate.audio.ssrc;
    const std::uint16_t audioRemotePort =
        separate.audio.transport.remoteRtpEndpoint().port();
    auto audioCreated = MediaScheduledRtpSenderNode::create(
        audioGraph.sender, runtimePlan.groupKey, std::move(separate.audio),
        separate.sdp,
        MediaScheduledRtpSenderNodeDependencies{
            active.group,
            std::make_unique<FakeSenderPortFactory>(audioRtp, audioRtcp),
            std::make_unique<FakePacketizerFactory>(audioPacketizer)});
    EXPECT_TRUE(ctx, audioCreated);
    if (!audioCreated) return;
    auto audioNode = std::move(audioCreated).value();
    EXPECT_TRUE(ctx, audioNode->start(audioGraph.execution));
    auto audioActivation = MediaPlaybackEpochActivatedBuffer::create(
        runtimePlan.groupKey,
        {milliseconds(0), milliseconds(0), 1},
        {1, milliseconds(0), milliseconds(0), 0, 48'000},
        std::nullopt);
    auto audioCodec = makeMediaBufferRef<FFmpegCodecContextBuffer>(
        codecContext(MediaScheduledStream::Audio));
    EXPECT_TRUE(ctx, audioActivation && audioCodec);
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.sender, "epoch")
                         ->push(std::move(audioActivation).value()));
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.sender, "codec")->push(audioCodec));
    EXPECT_TRUE(ctx, audioNode->process(audioGraph.execution));
    EXPECT_TRUE(ctx, audioNode->process(audioGraph.execution));
    EXPECT_EQ(ctx, audioPacketizer->stream, MediaStreamKind::Audio);
    EXPECT_EQ(ctx, audioPacketizer->mode,
              MediaScheduledRtpPacketizationMode::AacLatm);
    EXPECT_EQ(ctx, audioPacketizer->payloadType, 97);
    MediaBufferRef audioDescription;
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.descriptionSink, "description")
                         ->tryPop(audioDescription));
    const auto* typedAudio =
        dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
            audioDescription.get());
    EXPECT_TRUE(ctx, typedAudio != nullptr);
    if (typedVideo && typedAudio) {
        EXPECT_EQ(ctx, typedAudio->stream(), MediaScheduledStream::Audio);
        EXPECT_EQ(ctx, typedVideo->session().sessionId(),
                  typedAudio->session().sessionId());
        EXPECT_EQ(ctx, typedVideo->session().sessionVersion(),
                  std::uint64_t{1});
        EXPECT_EQ(ctx, typedVideo->session().sessionVersion(),
                  typedAudio->session().sessionVersion());
        EXPECT_EQ(ctx, typedVideo->session().cname(),
                  typedAudio->session().cname());
        EXPECT_EQ(ctx, typedVideo->media().identity().ssrc(), videoSsrc);
        EXPECT_EQ(ctx, typedAudio->media().identity().ssrc(), audioSsrc);
        EXPECT_FALSE(ctx, videoSsrc == audioSsrc);
        EXPECT_EQ(ctx, typedVideo->media().identity().remoteRtpPort(),
                  videoRemotePort);
        EXPECT_EQ(ctx, typedAudio->media().identity().remoteRtpPort(),
                  audioRemotePort);
        EXPECT_FALSE(ctx, videoRemotePort == audioRemotePort);
    }

    auto videoUnit = scheduledUnit(
        MediaScheduledStream::Video, videoLead, 1);
    auto audioUnit = scheduledUnit(
        MediaScheduledStream::Audio, audioLead, 2);
    EXPECT_TRUE(ctx, videoUnit && audioUnit);
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(
                         graph.sender, "scheduled")
                         ->push(std::move(videoUnit).value()));
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.sender, "scheduled")
                         ->push(std::move(audioUnit).value()));
    EXPECT_TRUE(ctx, node->process(graph.execution));
    EXPECT_TRUE(ctx, audioNode->process(audioGraph.execution));
    EXPECT_EQ(ctx, packetizer->writeCalls, 1);
    EXPECT_EQ(ctx, audioPacketizer->writeCalls, 1);
    EXPECT_EQ(ctx, rtp->sendCalls, 1);
    EXPECT_EQ(ctx, audioRtp->sendCalls, 1);

    auto backloggedVideo = scheduledUnit(
        MediaScheduledStream::Video, videoLead, 3);
    auto backloggedAudio = scheduledUnit(
        MediaScheduledStream::Audio, audioLead, 4);
    EXPECT_TRUE(ctx, backloggedVideo && backloggedAudio);
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(
                         graph.sender, "scheduled")
                         ->push(std::move(backloggedVideo).value()));
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.sender, "scheduled")
                         ->push(std::move(backloggedAudio).value()));
    active.clock->set(videoReportInterval);
    EXPECT_TRUE(ctx, node->process(graph.execution));
    active.clock->set(audioReportInterval);
    EXPECT_TRUE(ctx, audioNode->process(audioGraph.execution));
    EXPECT_EQ(ctx, rtcp->sendCalls, 1);
    EXPECT_EQ(ctx, audioRtcp->sendCalls, 1);
    EXPECT_EQ(ctx, packetizer->writeCalls, 1);
    EXPECT_EQ(ctx, audioPacketizer->writeCalls, 1);
    EXPECT_TRUE(ctx, node->process(graph.execution));
    EXPECT_TRUE(ctx, audioNode->process(audioGraph.execution));
    EXPECT_EQ(ctx, packetizer->writeCalls, 2);
    EXPECT_EQ(ctx, audioPacketizer->writeCalls, 2);

    auto videoEof = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    auto audioEof = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    EXPECT_TRUE(ctx, graph.execution.findInputChannel(
                         graph.sender, "scheduled")->push(videoEof));
    EXPECT_TRUE(ctx, audioGraph.execution.findInputChannel(
                         audioGraph.sender, "scheduled")->push(audioEof));
    auto videoFinished = node->process(graph.execution);
    auto audioFinished = audioNode->process(audioGraph.execution);
    EXPECT_TRUE(ctx, videoFinished &&
                         videoFinished.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_TRUE(ctx, audioFinished &&
                         audioFinished.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_TRUE(ctx, node->stop(graph.execution));
    EXPECT_TRUE(ctx, audioNode->stop(audioGraph.execution));
    EXPECT_TRUE(ctx, rtp->closeCalls > 0);
    EXPECT_TRUE(ctx, rtcp->closeCalls > 0);
}

void testSenderFailureAndLifecycleMatrix(TestContext& ctx)
{
    auto outer = MediaRealtimeRtpTranscodePlanner::plan(completeRequest());
    EXPECT_TRUE(ctx, outer && outer.value().avSyncRuntime);
    if (!outer || !outer.value().avSyncRuntime) return;
    auto runtimePlan = std::move(*outer.value().avSyncRuntime);
    auto active = activeGroup(ctx, runtimePlan);
    if (!active.group) return;

    auto missingCodec = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Video,
        false, 43'000);
    if (!missingCodec ||
        !pushActivationAndCodec(
            ctx, *missingCodec, runtimePlan.groupKey, false)) {
        return;
    }
    auto acquiredEpoch = missingCodec->node->process(
        missingCodec->graph.execution);
    auto waitingForCodec = missingCodec->node->process(
        missingCodec->graph.execution);
    EXPECT_TRUE(ctx, acquiredEpoch &&
                         acquiredEpoch.value().state ==
                             MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, waitingForCodec &&
                         waitingForCodec.value().state ==
                             MediaNodeProcessState::Waiting);
    missingCodec->graph.execution.findInputChannel(
        missingCodec->graph.sender, "codec")->close();
    EXPECT_FALSE(ctx, missingCodec->node->process(
                          missingCodec->graph.execution));
    EXPECT_EQ(ctx, missingCodec->rtp->openCalls, 0);
    EXPECT_TRUE(ctx, missingCodec->node->stop(
                         missingCodec->graph.execution));

    auto invalidCodec = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Video,
        false, 43'100);
    if (!invalidCodec) return;
    auto invalidActivation = MediaPlaybackEpochActivatedBuffer::create(
        runtimePlan.groupKey,
        {milliseconds(0), milliseconds(0), 1},
        {1, milliseconds(0), milliseconds(0), 0, 48'000},
        std::nullopt);
    auto invalidContext = codecContext(MediaScheduledStream::Video);
    EXPECT_TRUE(ctx, invalidActivation && invalidContext);
    if (!invalidActivation || !invalidContext) return;
    av_freep(&invalidContext->extradata);
    invalidContext->extradata_size = 0;
    auto invalidMetadata = makeMediaBufferRef<FFmpegCodecContextBuffer>(
        std::move(invalidContext));
    EXPECT_TRUE(ctx, invalidCodec->graph.execution.findInputChannel(
                         invalidCodec->graph.sender, "epoch")
                         ->push(std::move(invalidActivation).value()));
    EXPECT_TRUE(ctx, invalidCodec->graph.execution.findInputChannel(
                         invalidCodec->graph.sender, "codec")
                         ->push(invalidMetadata));
    EXPECT_FALSE(ctx, invalidCodec->node->process(
                          invalidCodec->graph.execution));
    EXPECT_EQ(ctx, invalidCodec->rtp->openCalls, 0);
    EXPECT_EQ(ctx, invalidCodec->rtcp->openCalls, 0);
    EXPECT_EQ(ctx, invalidCodec->packetizer->createCalls, 0);
    EXPECT_TRUE(ctx, invalidCodec->node->stop(
                         invalidCodec->graph.execution));

    auto rollback = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Audio,
        true, 44'000);
    if (!rollback ||
        !pushActivationAndCodec(ctx, *rollback, runtimePlan.groupKey, true)) {
        return;
    }
    EXPECT_FALSE(ctx, rollback->node->process(rollback->graph.execution));
    EXPECT_EQ(ctx, rollback->packetizer->openCalls, 1);
    EXPECT_EQ(ctx, rollback->rtp->openCalls, 1);
    EXPECT_EQ(ctx, rollback->rtcp->openCalls, 1);
    EXPECT_TRUE(ctx, rollback->rtp->closeCalls > 0);
    EXPECT_TRUE(ctx, rollback->rtcp->closeCalls > 0);
    EXPECT_TRUE(ctx, rollback->node->stop(rollback->graph.execution));

    auto pendingDescription = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Video,
        false, 44'100, 1);
    if (!pendingDescription ||
        !pushActivationAndCodec(
            ctx, *pendingDescription, runtimePlan.groupKey, true)) {
        return;
    }
    auto blocker = description(MediaScheduledStream::Video);
    EXPECT_TRUE(ctx, blocker);
    if (!blocker) return;
    EXPECT_TRUE(ctx, pendingDescription->graph.execution.findOutputChannel(
                         pendingDescription->graph.sender, "description")
                         ->push(std::move(blocker).value()));
    auto deferred = pendingDescription->node->process(
        pendingDescription->graph.execution);
    EXPECT_TRUE(ctx, deferred &&
                         deferred.value().state ==
                             MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, pendingDescription->node->flush(
                         pendingDescription->graph.execution));
    MediaBufferRef drained;
    auto* descriptionInput =
        pendingDescription->graph.execution.findInputChannel(
            pendingDescription->graph.descriptionSink, "description");
    EXPECT_TRUE(ctx, descriptionInput->tryPop(drained));
    auto afterFlush = pendingDescription->node->process(
        pendingDescription->graph.execution);
    EXPECT_TRUE(ctx, afterFlush &&
                         afterFlush.value().state ==
                             MediaNodeProcessState::Waiting);
    EXPECT_FALSE(ctx, descriptionInput->tryPop(drained));
    EXPECT_TRUE(ctx, pendingDescription->node->stop(
                         pendingDescription->graph.execution));

    auto blocked = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Video,
        false, 45'000);
    if (!blocked ||
        !pushActivationAndCodec(ctx, *blocked, runtimePlan.groupKey, true)) {
        return;
    }
    EXPECT_TRUE(ctx, blocked->node->process(blocked->graph.execution));
    EXPECT_TRUE(ctx, blocked->node->process(blocked->graph.execution));
    blocked->rtp->outcomes.push_back(
        MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::wouldBlock("scripted RTP pressure")));
    auto accessUnit = scheduledUnit(
        MediaScheduledStream::Video, blocked->senderLead, 11);
    EXPECT_TRUE(ctx, accessUnit);
    if (!accessUnit) return;
    EXPECT_TRUE(ctx, blocked->graph.execution.findInputChannel(
                         blocked->graph.sender, "scheduled")
                         ->push(std::move(accessUnit).value()));
    EXPECT_FALSE(ctx, blocked->node->process(blocked->graph.execution));
    EXPECT_EQ(ctx, blocked->rtp->sendCalls, 1);
    EXPECT_FALSE(ctx, blocked->node->process(blocked->graph.execution));
    EXPECT_EQ(ctx, blocked->rtp->sendCalls, 1);
    EXPECT_TRUE(ctx, blocked->rtp->closeCalls > 0);
    EXPECT_TRUE(ctx, blocked->node->stop(blocked->graph.execution));

    auto reset = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Audio,
        false, 46'000);
    if (!reset ||
        !pushActivationAndCodec(ctx, *reset, runtimePlan.groupKey, true)) {
        return;
    }
    EXPECT_TRUE(ctx, reset->node->process(reset->graph.execution));
    EXPECT_TRUE(ctx, reset->node->process(reset->graph.execution));
    EXPECT_TRUE(ctx, reset->node->flush(reset->graph.execution));
    EXPECT_TRUE(ctx, reset->rtp->closeCalls > 0);
    auto afterReset = reset->node->process(reset->graph.execution);
    EXPECT_TRUE(ctx, afterReset &&
                         afterReset.value().state ==
                             MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, reset->node->stop(reset->graph.execution));

    auto aborted = senderCase(
        ctx, runtimePlan, active.group, MediaScheduledStream::Video,
        false, 47'000);
    if (!aborted ||
        !pushActivationAndCodec(ctx, *aborted, runtimePlan.groupKey, true)) {
        return;
    }
    EXPECT_TRUE(ctx, aborted->node->process(aborted->graph.execution));
    EXPECT_TRUE(ctx, aborted->node->process(aborted->graph.execution));
    aborted->node->abort(aborted->graph.execution);
    EXPECT_EQ(ctx, active.group->lifecycleState(),
              MediaAvSyncGroupRuntime::LifecycleState::Aborted);
    EXPECT_TRUE(ctx, aborted->rtp->closeCalls > 0);
    EXPECT_FALSE(ctx, aborted->node->process(aborted->graph.execution));
}

} // namespace

void runScheduledRtpSenderNodeTests(TestContext& ctx)
{
    testPermitCloseDropsOldGenerationAndReusesTransport(ctx);
    testActivationOpensExactPlannedSenders(ctx);
    testSenderFailureAndLifecycleMatrix(ctx);
}

} // namespace media_transcode::test::scheduled_rtp_output
