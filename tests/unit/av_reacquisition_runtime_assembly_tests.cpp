#include "common/TestAssert.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaScheduledRtpSenderNodeTestAccess final {
    static void replaceGenerationPurgeIdentity(
        MediaScheduledRtpSenderNode& node,
        std::string identity)
    {
        node.m_generationState =
            std::make_shared<MediaProtocolOutputGenerationState>(
                std::move(identity));
    }
};

} // namespace media::ffmpeg::graph

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class AcceptingPurgeTarget final : public MediaAvGenerationPurgeTarget {
public:
    ::media::Status purge(const MediaAvGenerationPurge&) override
    {
        return ::media::Status::success();
    }
};

class NonParticipantRuntimeNode final : public MediaRuntimeNode {
public:
    explicit NonParticipantRuntimeNode(MediaNodeId nodeId)
        : m_nodeId(nodeId)
    {
    }

    MediaNodeId nodeId() const noexcept override { return m_nodeId; }

    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext&) override
    {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::finished());
    }

private:
    MediaNodeId m_nodeId;
};

MediaAvGenerationTransitionPlan separateRtpPlan()
{
    return MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
        ms(500),
        ms(100));
}

MediaRealtimeRtpTranscodeRequest productionRequest(
    MediaAvSyncOutputAdapterKind outputAdapter)
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = outputAdapter ==
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp
        ? "assembly-production-rtp"
        : "assembly-production-mpegts";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:45004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets="
        "Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;"
        "profile-level-id=4D4032";
    request.input.audioRtp.url = "rtp://127.0.0.1:45006";
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1190;"
        "sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.packetSize = 1'200;
    if (outputAdapter ==
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        request.output.streamLayout =
            RealtimeOutputStreamLayout::SeparateStreams;
        request.output.host = "127.0.0.1";
        request.output.basePort = 46004;
        request.output.sdpPath = "assembly-production.sdp";
    } else {
        request.input.type = RealtimeInputType::MpegTsUdp;
        request.input.streamLayout =
            RealtimeInputStreamLayout::MuxedTransportStream;
        request.input.url = "udp://127.0.0.1:45008";
        request.output.streamLayout =
            RealtimeOutputStreamLayout::MuxedTransportStream;
        request.output.url = "udp://127.0.0.1:46008";
    }
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.queues.metadata = 8;
    request.parameters.queues.packet = 256;
    request.parameters.queues.frame = 128;
    request.parameters.queues.mux = 256;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'406;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.audio.channels = 2;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    return request;
}

MediaRealtimeInputStreamInfo productionMpegTsStreams()
{
    MediaRealtimeInputStreamInfo streams;
    streams.video.streamIndex = 3;
    streams.video.codecName = "h264";
    streams.video.width = 1'920;
    streams.video.height = 1'080;
    streams.video.bitrateBitsPerSecond = 8'000'000;
    streams.video.frameRate = {30'000, 1'001};
    streams.hasAudio = true;
    streams.audio.streamIndex = 5;
    streams.audio.codecName = "aac";
    streams.audio.sampleRate = 48'000;
    streams.audio.channels = 2;
    streams.audio.channelLayout = "stereo";
    streams.audio.sampleFormat = "fltp";
    streams.audio.profile = MediaAudioProfile::knownAacLow();
    streams.audio.bitrateBitsPerSecond = 320'000;
    streams.audio.maximumAccessUnitSamples = 1'024;
    streams.audio.selectedDecoder = MediaSelectedAudioDecoder{
        "aac", "fltp", "stereo", 48'000, 48'000, 2, 0, 1'024};
    return streams;
}

MediaTsSelectedProgramPlan productionMpegTsSelection()
{
    MediaTsSelectedProgramPlan selected{7, 777, 703, 705, 701};
    selected.videoPacketDuration =
        MediaTsPacketDurationEvidence{3, 703, 3'003, {1, 90'000}};
    selected.audioPacketDuration =
        MediaTsPacketDurationEvidence{5, 705, 1'024, {1, 48'000}};
    return selected;
}

::media::Result<MediaRealtimeExecutableGraph> productionRtpExecutable()
{
    auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        productionRequest(
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp));
    if (!planned) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            planned.error());
    }
    return MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
        MediaRealtimeTranscodePreflight{
            std::move(planned).value(), std::nullopt});
}

struct ProductionMpegTsGraph final {
    MediaGraph graph;
    MediaAvGenerationTransitionPlan transition;
};

::media::Result<ProductionMpegTsGraph> productionMpegTsGraph()
{
    auto planned = MediaRealtimeRtpTranscodePlanner::planPreparedInput(
        productionRequest(MediaAvSyncOutputAdapterKind::ProjectMpegTs),
        productionMpegTsStreams(),
        productionMpegTsSelection());
    if (!planned) {
        return ::media::Result<ProductionMpegTsGraph>::failure(
            planned.error());
    }
    auto plan = std::move(planned).value();
    if (!plan.avSyncRuntime) {
        return ::media::Result<ProductionMpegTsGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "production MPEG-TS plan requires A/V runtime binding"));
    }
    auto transition = plan.avSyncRuntime->transition;
    auto graph = MediaRealtimeRtpTranscodeGraphBuilder::build(
        std::move(plan));
    if (!graph) {
        return ::media::Result<ProductionMpegTsGraph>::failure(
            graph.error());
    }
    return ::media::Result<ProductionMpegTsGraph>::success(
        ProductionMpegTsGraph{
            std::move(graph).value(), std::move(transition)});
}

void registerCompletePlan(
    TestContext& ctx,
    MediaAvGenerationParticipantAssembler& assembler,
    const MediaAvGenerationTransitionPlan& plan)
{
    for (const auto& participant : plan.participants) {
        for (const auto& identity : participant.requiredChildren) {
            EXPECT_TRUE(
                ctx,
                assembler.registerTarget(
                    participant.participant,
                    MediaAvGenerationPurgeRegistration{
                        identity, std::make_shared<AcceptingPurgeTarget>()}));
        }
    }
}

void productionFactoryRegistrationsSeal(
    TestContext& ctx,
    MediaAvSyncOutputAdapterKind outputAdapter)
{
    std::optional<MediaRealtimeExecutableGraph> rtpExecutable;
    std::optional<ProductionMpegTsGraph> mpegTsGraph;
    std::optional<MediaAvGenerationTransitionPlan> transition;
    if (outputAdapter ==
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        auto executable = productionRtpExecutable();
        EXPECT_TRUE(ctx, executable);
        if (!executable || !executable.value().avSyncBinding) return;
        transition.emplace(
            executable.value().avSyncBinding->transition);
        rtpExecutable.emplace(std::move(executable).value());
    } else {
        auto graph = productionMpegTsGraph();
        EXPECT_TRUE(ctx, graph);
        if (!graph) return;
        transition.emplace(graph.value().transition);
        mpegTsGraph.emplace(std::move(graph).value());
    }

    auto created = MediaAvGenerationParticipantAssembler::create(*transition);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    std::size_t rtpVideoRegistrations = 0;
    std::size_t rtpAudioRegistrations = 0;
    std::size_t mpegTsRegistrations = 0;
    MediaScheduledRtpSenderNode* audioSender = nullptr;
    const auto registerRuntimeNode = [&](MediaRuntimeNode& runtimeNode) {
        auto registration =
            MediaRuntimeNodeFactory::generationPurgeRegistration(
                runtimeNode);
        if (!registration) return;
        if (registration->participant ==
            MediaAvGenerationParticipant::RtpVideoOutput) {
            ++rtpVideoRegistrations;
            EXPECT_EQ(
                ctx, registration->registration.identity,
                std::string("rtp_video_output_generation_state"));
        } else if (registration->participant ==
                   MediaAvGenerationParticipant::RtpAudioOutput) {
            ++rtpAudioRegistrations;
            EXPECT_EQ(
                ctx, registration->registration.identity,
                std::string("rtp_audio_output_generation_state"));
            audioSender =
                dynamic_cast<MediaScheduledRtpSenderNode*>(&runtimeNode);
        } else if (registration->participant ==
                   MediaAvGenerationParticipant::ProjectMpegTsOutput) {
            ++mpegTsRegistrations;
            EXPECT_EQ(
                ctx, registration->registration.identity,
                std::string("project_mpegts_output_generation_state"));
        }
        EXPECT_TRUE(
            ctx,
            assembler.registerTarget(
                registration->participant,
                std::move(registration->registration)));
    };

    MediaGraphRuntime runtime;
    std::vector<std::unique_ptr<MediaRuntimeNode>> standaloneRuntimeNodes;
    if (outputAdapter ==
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        EXPECT_TRUE(ctx, runtime.compile(std::move(*rtpExecutable)));
        EXPECT_EQ(ctx, runtime.state(),
                  MediaGraphRuntimeState::DefaultRegistrationPending);
        EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
        EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Compiled);
        for (auto* runtimeNode :
             runtime.scheduler().orderedRuntimeNodes(runtime.context())) {
            if (runtimeNode) registerRuntimeNode(*runtimeNode);
        }
    } else {
        for (const auto& node : mpegTsGraph->graph.nodes()) {
            if (node.kind == MediaNodeKind::RealtimeInput ||
                node.kind ==
                    MediaNodeKind::ActivatedStartupReleaseSequencer) {
                continue;
            }
            auto runtimeNode = MediaRuntimeNodeFactory::create(node);
            EXPECT_TRUE(ctx, runtimeNode);
            if (!runtimeNode) continue;
            registerRuntimeNode(*runtimeNode.value());
            standaloneRuntimeNodes.push_back(
                std::move(runtimeNode).value());
        }
    }
    auto sealed = assembler.seal();
    EXPECT_TRUE(ctx, sealed);
    if (outputAdapter ==
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        EXPECT_EQ(ctx, rtpVideoRegistrations, std::size_t{1});
        EXPECT_EQ(ctx, rtpAudioRegistrations, std::size_t{1});
        EXPECT_EQ(ctx, mpegTsRegistrations, std::size_t{0});
        EXPECT_TRUE(ctx, audioSender != nullptr);
        if (audioSender) {
            MediaScheduledRtpSenderNodeTestAccess::
                replaceGenerationPurgeIdentity(
                    *audioSender, "unknown_rtp_output_generation_state");
            auto invalid =
                MediaRuntimeNodeFactory::generationPurgeRegistration(
                    *audioSender);
            EXPECT_FALSE(ctx, invalid);
        }
    } else {
        EXPECT_EQ(ctx, rtpVideoRegistrations, std::size_t{0});
        EXPECT_EQ(ctx, rtpAudioRegistrations, std::size_t{0});
        EXPECT_EQ(ctx, mpegTsRegistrations, std::size_t{1});
    }
}

void productionMissingTargetAbortsBeforeCompiled(TestContext& ctx)
{
    auto executable = productionRtpExecutable();
    EXPECT_TRUE(ctx, executable);
    if (!executable) return;
    MediaNodeId schedulerId = MediaNodeId::invalid();
    for (const auto& node : executable.value().graph.nodes()) {
        if (node.kind == MediaNodeKind::AvOutputScheduler) {
            schedulerId = node.id;
            break;
        }
    }
    EXPECT_TRUE(ctx, schedulerId.isValid());
    if (!schedulerId.isValid()) return;

    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(executable).value()));
    EXPECT_EQ(ctx, runtime.state(),
              MediaGraphRuntimeState::DefaultRegistrationPending);
    EXPECT_TRUE(
        ctx,
        runtime.registerRuntimeNode(
            std::make_unique<NonParticipantRuntimeNode>(schedulerId)));
    const auto registered = runtime.registerDefaultRuntimeNodes();
    EXPECT_FALSE(ctx, registered);
    if (!registered) {
        EXPECT_EQ(ctx, registered.error().code,
                  ::media::ErrorCode::InvalidArgument);
        EXPECT_EQ(
            ctx, registered.error().message,
            std::string(
                "Generation participant can seal exactly once with its complete child set"));
    }
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Aborted);
    EXPECT_FALSE(ctx, runtime.compiled());
    EXPECT_FALSE(ctx, runtime.startThreaded());
}

void exactPlannerProductSeals(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    registerCompletePlan(ctx, assembler, plan);

    auto sealed = assembler.seal();
    EXPECT_TRUE(ctx, sealed);
    if (sealed) {
        EXPECT_EQ(ctx, sealed.value().size(), plan.participants.size());
    }
    EXPECT_FALSE(ctx, assembler.seal());
}

void missingRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    bool skipped = false;
    for (const auto& participant : plan.participants) {
        for (const auto& identity : participant.requiredChildren) {
            if (!skipped) {
                skipped = true;
                continue;
            }
            EXPECT_TRUE(
                ctx,
                assembler.registerTarget(
                    participant.participant,
                    {identity, std::make_shared<AcceptingPurgeTarget>()}));
        }
    }
    EXPECT_FALSE(ctx, assembler.seal());
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            plan.participants.front().participant,
            {plan.participants.front().requiredChildren.front(),
             std::make_shared<AcceptingPurgeTarget>()}));
    EXPECT_FALSE(ctx, assembler.seal());
}

void duplicateRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    const auto& participant = plan.participants.front();
    const auto& identity = participant.requiredChildren.front();
    EXPECT_TRUE(
        ctx,
        assembler.registerTarget(
            participant.participant,
            {identity, std::make_shared<AcceptingPurgeTarget>()}));
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            participant.participant,
            {identity, std::make_shared<AcceptingPurgeTarget>()}));
}

void unexpectedRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"not_planned", std::make_shared<AcceptingPurgeTarget>()}));
}

void emptyRegistrationIsRejected(TestContext& ctx)
{
    auto created =
        MediaAvGenerationParticipantAssembler::create(separateRtpPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"", std::make_shared<AcceptingPurgeTarget>()}));
}

void nullRegistrationIsRejected(TestContext& ctx)
{
    auto created =
        MediaAvGenerationParticipantAssembler::create(separateRtpPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"scheduler_generation_state", nullptr}));
}

} // namespace

int main()
{
    TestContext ctx;
    productionFactoryRegistrationsSeal(
        ctx, MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp);
    productionFactoryRegistrationsSeal(
        ctx, MediaAvSyncOutputAdapterKind::ProjectMpegTs);
    productionMissingTargetAbortsBeforeCompiled(ctx);
    exactPlannerProductSeals(ctx);
    missingRegistrationIsRejected(ctx);
    duplicateRegistrationIsRejected(ctx);
    unexpectedRegistrationIsRejected(ctx);
    emptyRegistrationIsRejected(ctx);
    nullRegistrationIsRejected(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
