#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <cstdint>
#include <string>
#include <variant>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaRealtimeRtpTranscodeRequest completeProductionRtpRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "production-assembly";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:5004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    request.input.audioRtp.url = "rtp://127.0.0.1:5006";
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.output.host = "127.0.0.1";
    request.output.basePort = 6000;
    request.output.sdpPath = "production-assembly.sdp";
    request.output.packetSize = 1200;
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'000;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.channels = 2;
    request.parameters.queues.metadata = 4;
    request.parameters.queues.packet = 4;
    request.parameters.queues.frame = 4;
    request.parameters.queues.mux = 4;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap =
        MediaRunningTime::fromNanoseconds(40'000'000);
    return request;
}

void expectInvalid(TestContext& ctx, const MediaRealtimeRtpTranscodePlan& plan)
{
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(plan));
}

void testCompleteSeparateRtpAssemblyProduct(TestContext& ctx)
{
    auto result = MediaRealtimeRtpTranscodePlanner::plan(
        completeProductionRtpRequest());
    EXPECT_TRUE(ctx, result);
    if (!result || !result.value().avSyncRuntime) return;

    const auto& runtime = *result.value().avSyncRuntime;
    const auto& assembly = runtime.assembly;
    EXPECT_TRUE(ctx, std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
                         assembly.inputClock));
    EXPECT_EQ(ctx,
              std::get<MediaRtpInputClockAssemblyPlan>(assembly.inputClock)
                  .commonEpochPolicy,
              MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime);
    EXPECT_EQ(ctx, assembly.generationPolicy,
              MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange);
    EXPECT_EQ(ctx, assembly.evidencePolicy,
              MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire);
    EXPECT_EQ(ctx, assembly.video.sourceIdentity,
              std::string("production-assembly.input.video"));
    EXPECT_EQ(ctx, assembly.audio.sourceIdentity,
              std::string("production-assembly.input.audio"));
    EXPECT_EQ(ctx, assembly.video.decodeOrder,
              MediaDecodeOrderMode::ReorderedRequiresDecodeTime);
    EXPECT_EQ(ctx, assembly.audio.decodeOrder,
              MediaDecodeOrderMode::PresentationOrderNoReorder);
    EXPECT_EQ(ctx, assembly.video.acquiringCapacity, std::size_t{4});
    EXPECT_EQ(ctx, assembly.audio.acquiringCapacity, std::size_t{4});
    EXPECT_TRUE(ctx, assembly.video.acquiringTimeout >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, assembly.audio.acquiringTimeout >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, assembly.startupClockInterval >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx,
                std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(
                    assembly.video.duration));
    const auto& videoDuration =
        std::get<MediaRtpTimestampDeltaDurationPlan>(assembly.video.duration);
    EXPECT_EQ(ctx, videoDuration.clockRate, 90'000);
    EXPECT_EQ(ctx, videoDuration.terminalPolicy,
              MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta);
    EXPECT_TRUE(ctx,
                std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
                    assembly.audio.duration));
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(assembly.audio.duration);
    EXPECT_EQ(ctx, audioDuration.sampleRate, 48'000);
    EXPECT_EQ(ctx, audioDuration.samplesPerAccessUnit, std::uint32_t{1024});
}

void testAssemblyRejectsEveryInvalidContractField(TestContext& ctx)
{
    auto result = MediaRealtimeRtpTranscodePlanner::plan(
        completeProductionRtpRequest());
    EXPECT_TRUE(ctx, result);
    if (!result || !result.value().avSyncRuntime) return;
    auto plan = std::move(result).value();
    const auto valid = plan.avSyncRuntime->assembly;

    plan.avSyncRuntime->assembly.video.sourceIdentity.clear();
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.audio.sourceIdentity.clear();
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    std::get<MediaRtpTimestampDeltaDurationPlan>(
        plan.avSyncRuntime->assembly.video.duration).clockRate = 0;
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    std::get<MediaRtpTimestampDeltaDurationPlan>(
        plan.avSyncRuntime->assembly.video.duration).terminalPolicy =
        static_cast<MediaTerminalDurationPolicy>(255);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.video.duration = MediaPacketDurationPlan{true};
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    std::get<MediaPlannedAudioSamplesDurationPlan>(
        plan.avSyncRuntime->assembly.audio.duration).sampleRate = 0;
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    std::get<MediaPlannedAudioSamplesDurationPlan>(
        plan.avSyncRuntime->assembly.audio.duration).samplesPerAccessUnit = 0;
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.audio.duration = MediaPacketDurationPlan{true};
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.video.acquiringCapacity = 0;
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.audio.acquiringCapacity = 0;
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.video.acquiringTimeout =
        MediaRunningTime::fromNanoseconds(0);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.audio.acquiringTimeout =
        MediaRunningTime::fromNanoseconds(0);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.startupClockInterval =
        MediaRunningTime::fromNanoseconds(0);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.generationPolicy =
        static_cast<MediaInitialGenerationPolicy>(255);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->assembly.evidencePolicy =
        static_cast<MediaClockEvidencePolicy>(255);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    std::get<MediaRtpInputClockAssemblyPlan>(
        plan.avSyncRuntime->assembly.inputClock).commonEpochPolicy =
        static_cast<MediaRtpCommonEpochPolicy>(255);
    expectInvalid(ctx, plan);
    plan.avSyncRuntime->assembly = valid;
    plan.avSyncRuntime->outputAdapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
    expectInvalid(ctx, plan);
}

void testCompleteMpegTsAssemblyProduct(TestContext& ctx)
{
    auto outerResult = MediaRealtimeRtpTranscodePlanner::plan(
        completeProductionRtpRequest());
    EXPECT_TRUE(ctx, outerResult);
    if (!outerResult) return;
    auto outer = std::move(outerResult).value();
    outer.inputType = RealtimeInputType::MpegTsUdp;
    outer.inputLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    outer.outputLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    outer.muxedOutput.url = "udp://127.0.0.1:7000";

    auto request = completeProductionRtpRequest();
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.input.url = "udp://127.0.0.1:5000";
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = outer.muxedOutput.url;
    const MediaTsSelectedProgramPlan selected{7, 777, 703, 705, 701};
    const MediaProjectMpegTsResolvedPipelineFacts resolved{
        outer.videoPlan.outputCodecName, *outer.audioPlan.resolvedOutput};
    auto synchronization = MediaAvSyncPlanner::plan(request, &selected, &resolved);
    EXPECT_TRUE(ctx, synchronization);
    if (!synchronization) return;
    auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
        outer, std::move(synchronization).value());
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    outer.avSyncRuntime = std::move(runtime).value();
    EXPECT_TRUE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    const auto& assembly = outer.avSyncRuntime->assembly;
    EXPECT_TRUE(ctx, std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
                         assembly.inputClock));
    EXPECT_EQ(ctx, assembly.generationPolicy,
              MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange);
    EXPECT_EQ(ctx, assembly.evidencePolicy,
              MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire);
    EXPECT_EQ(ctx, assembly.video.sourceIdentity,
              std::string("production-assembly.pid.703"));
    EXPECT_EQ(ctx, assembly.audio.sourceIdentity,
              std::string("production-assembly.pid.705"));
    EXPECT_EQ(ctx, assembly.video.decodeOrder,
              MediaDecodeOrderMode::ReorderedRequiresDecodeTime);
    EXPECT_EQ(ctx, assembly.audio.decodeOrder,
              MediaDecodeOrderMode::PresentationOrderNoReorder);
    EXPECT_EQ(ctx, assembly.video.acquiringCapacity, std::size_t{4});
    EXPECT_EQ(ctx, assembly.audio.acquiringCapacity, std::size_t{4});
    EXPECT_TRUE(ctx, assembly.video.acquiringTimeout >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, assembly.audio.acquiringTimeout >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, assembly.startupClockInterval >
                         MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, std::holds_alternative<MediaPacketDurationPlan>(
                         assembly.video.duration));
    EXPECT_TRUE(ctx, std::holds_alternative<MediaPacketDurationPlan>(
                         assembly.audio.duration));
    EXPECT_TRUE(ctx, std::get<MediaPacketDurationPlan>(assembly.video.duration)
                         .requirePositiveDuration);
    EXPECT_TRUE(ctx, std::get<MediaPacketDurationPlan>(assembly.audio.duration)
                         .requirePositiveDuration);

    const auto valid = assembly;
    std::get<MediaPacketDurationPlan>(
        outer.avSyncRuntime->assembly.video.duration).requirePositiveDuration = false;
    expectInvalid(ctx, outer);
    outer.avSyncRuntime->assembly = valid;
    std::get<MediaPacketDurationPlan>(
        outer.avSyncRuntime->assembly.audio.duration).requirePositiveDuration = false;
    expectInvalid(ctx, outer);
}

void testUnsupportedTopologyAndMissingSynchronizedAudioFailClosed(TestContext& ctx)
{
    auto request = completeProductionRtpRequest();
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = "udp://127.0.0.1:7000";
    EXPECT_FALSE(ctx, MediaAvSyncPlanner::plan(request, nullptr, nullptr));

    request = completeProductionRtpRequest();
    request.parameters.execution.includeAudio = false;
    EXPECT_FALSE(ctx, MediaAvSyncPlanner::plan(request, nullptr, nullptr));
}

} // namespace

void runAvSyncProductionPlanTests(TestContext& ctx)
{
    testCompleteSeparateRtpAssemblyProduct(ctx);
    testAssemblyRejectsEveryInvalidContractField(ctx);
    testCompleteMpegTsAssemblyProduct(ctx);
    testUnsupportedTopologyAndMissingSynchronizedAudioFailClosed(ctx);
}
