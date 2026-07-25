#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/protocol/mpegts/MediaTsPreflightDurationProbe.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsReadFrameEnvelope durationEnvelope(int streamIndex,
                                          std::int64_t duration)
{
    auto packet = media::ffmpeg::makePacket();
    packet->stream_index = streamIndex;
    packet->duration = duration;
    return MediaTsReadFrameEnvelope{
        MediaTsReadFrameState::Frame, std::move(packet), {}};
}

MediaTsPreflightDurationProbe completeDurationProbe(std::size_t frameLimit)
{
    auto probe = MediaTsPreflightDurationProbe::create(
        MediaTsRuntimeStreamBinding{3, 703}, MediaRational{1, 90'000},
        MediaTsRuntimeStreamBinding{5, 705}, MediaRational{1, 48'000},
        frameLimit);
    return std::move(probe).value();
}

void testTsDurationProbeOwnsExactEvidenceAndLosslessReplay(TestContext& ctx)
{
    static_assert(!std::is_copy_constructible_v<MediaTsPreflightDurationProbe>);
    static_assert(!std::is_copy_assignable_v<MediaTsPreflightDurationProbe>);
    static_assert(std::is_move_constructible_v<MediaTsPreflightDurationProbe>);

    auto probe = completeDurationProbe(4);
    auto unrelated = probe.buffer(durationEnvelope(9, 17));
    EXPECT_TRUE(ctx, unrelated);
    EXPECT_FALSE(ctx, unrelated.value().has_value());
    auto audio = probe.buffer(durationEnvelope(5, 1'024));
    EXPECT_TRUE(ctx, audio);
    EXPECT_FALSE(ctx, audio.value().has_value());
    auto video = probe.buffer(durationEnvelope(3, 3'003));
    EXPECT_TRUE(ctx, video);
    EXPECT_TRUE(ctx, video.value().has_value());
    if (!video || !video.value()) return;
    EXPECT_EQ(ctx, video.value()->video.streamIndex, 3);
    EXPECT_EQ(ctx, video.value()->video.elementaryPid, std::uint16_t{703});
    EXPECT_EQ(ctx, video.value()->video.packetDuration, std::int64_t{3'003});
    EXPECT_EQ(ctx, video.value()->video.timeBase.num, 1);
    EXPECT_EQ(ctx, video.value()->video.timeBase.den, 90'000);
    EXPECT_EQ(ctx, video.value()->audio.streamIndex, 5);
    EXPECT_EQ(ctx, video.value()->audio.elementaryPid, std::uint16_t{705});
    EXPECT_EQ(ctx, video.value()->audio.packetDuration, std::int64_t{1'024});
    EXPECT_EQ(ctx, video.value()->audio.timeBase.num, 1);
    EXPECT_EQ(ctx, video.value()->audio.timeBase.den, 48'000);

    for (const int expected : {9, 5, 3}) {
        auto replay = probe.popReplay();
        EXPECT_TRUE(ctx, replay);
        if (replay) EXPECT_EQ(ctx, replay.value().packet->stream_index, expected);
    }
    EXPECT_FALSE(ctx, probe.popReplay());
}

void testTsDurationProbeRejectsMissingAndNonPositiveEvidence(TestContext& ctx)
{
    auto zeroVideo = completeDurationProbe(2);
    EXPECT_FALSE(ctx, zeroVideo.buffer(durationEnvelope(3, 0)));
    auto negativeVideo = completeDurationProbe(2);
    EXPECT_FALSE(ctx, negativeVideo.buffer(durationEnvelope(3, -1)));
    auto zeroAudio = completeDurationProbe(2);
    EXPECT_FALSE(ctx, zeroAudio.buffer(durationEnvelope(5, 0)));
    auto negativeAudio = completeDurationProbe(2);
    EXPECT_FALSE(ctx, negativeAudio.buffer(durationEnvelope(5, -1)));

    auto missingAudio = completeDurationProbe(2);
    EXPECT_TRUE(ctx, missingAudio.buffer(durationEnvelope(9, 1)));
    EXPECT_FALSE(ctx, missingAudio.buffer(durationEnvelope(3, 3'003)));
    auto missingVideo = completeDurationProbe(2);
    EXPECT_TRUE(ctx, missingVideo.buffer(durationEnvelope(9, 1)));
    EXPECT_FALSE(ctx, missingVideo.buffer(durationEnvelope(5, 1'024)));
}

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

MediaRealtimeRtpTranscodeRequest completeProductionTsRequest()
{
    auto request = completeProductionRtpRequest();
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.input.url = "udp://127.0.0.1:5000";
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = "udp://127.0.0.1:7000";
    return request;
}

MediaRealtimeInputStreamInfo completeProductionTsStreams()
{
    MediaRealtimeInputStreamInfo streams;
    streams.video.streamIndex = 3;
    streams.video.codecName = "h264";
    streams.video.width = 1920;
    streams.video.height = 1080;
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

MediaTsSelectedProgramPlan completeProductionTsSelection()
{
    MediaTsSelectedProgramPlan selected{7, 777, 703, 705, 701};
    selected.videoPacketDuration = MediaTsPacketDurationEvidence{
        3, 703, 3'003, {1, 90'000}};
    selected.audioPacketDuration = MediaTsPacketDurationEvidence{
        5, 705, 1'024, {1, 48'000}};
    return selected;
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
    EXPECT_EQ(ctx, runtime.edgePolicies.videoFrame.queuePolicy.capacity,
              std::size_t{4});
    EXPECT_EQ(ctx,
              runtime.edgePolicies.videoFrame.queuePolicy.overflowPolicy,
              MediaQueueOverflowPolicy::DropOldest);
    EXPECT_TRUE(ctx, MediaAtomicOutputPolicyContract::accepts(
                         runtime.edgePolicies.preparedVideoFrame));
    EXPECT_TRUE(ctx, MediaAtomicOutputPolicyContract::accepts(
                         runtime.edgePolicies.synchronizedPacket));
    EXPECT_EQ(ctx, runtime.edgePolicies.audioFrame.queuePolicy.capacity,
              std::size_t{4});
    EXPECT_EQ(ctx,
              runtime.edgePolicies.audioFrame.queuePolicy.orderingPolicy,
              MediaQueueOrderingPolicy::Fifo);
    EXPECT_EQ(ctx,
              runtime.edgePolicies.audioFrame.queuePolicy.overflowPolicy,
              MediaQueueOverflowPolicy::BlockProducer);
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

    const auto& output =
        std::get<MediaSeparateRtpOutputRuntimePlan>(runtime.protocolOutput);
    EXPECT_EQ(ctx, output.sdp.path,
              std::string("production-assembly.sdp"));
    EXPECT_EQ(ctx, output.sdp.originUsername,
              std::string("production-assembly"));
    EXPECT_EQ(ctx, output.sdp.sessionName,
              std::string("production-assembly"));
    EXPECT_EQ(ctx, output.sdp.originAddressFamily,
              MediaIpAddressFamily::Ipv4);
    EXPECT_EQ(ctx, output.sdp.originNumericAddress,
              std::string("127.0.0.1"));
    EXPECT_EQ(ctx, output.sdp.cname,
              output.video.cname);
    EXPECT_EQ(ctx, output.sdp.cname,
              output.audio.cname);
    EXPECT_EQ(ctx, output.sdp.sessionIdPolicy,
              MediaRtpSdpSessionIdPolicy::SharedNtpEpoch);
    EXPECT_EQ(ctx, output.sdp.sessionVersionPolicy,
              MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration);
}

void testSeparateRtpSdpIdentityIsPlannerOwnedAndValidated(TestContext& ctx)
{
    auto invalidRequest = completeProductionRtpRequest();
    invalidRequest.mediaId = "invalid media id";
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::plan(invalidRequest));

    auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        completeProductionRtpRequest());
    EXPECT_TRUE(ctx, planned);
    if (!planned || !planned.value().avSyncRuntime) return;
    auto plan = std::move(planned).value();
    auto& output = std::get<MediaSeparateRtpOutputRuntimePlan>(
        plan.avSyncRuntime->protocolOutput);
    const auto valid = output.sdp;

    output.sdp.originUsername.clear();
    expectInvalid(ctx, plan);
    output.sdp = valid;
    output.sdp.sessionName.clear();
    expectInvalid(ctx, plan);
    output.sdp = valid;
    output.sdp.originNumericAddress = "127.0.0.2";
    expectInvalid(ctx, plan);
    output.sdp = valid;
    output.sdp.cname = "different-cname";
    expectInvalid(ctx, plan);
    output.sdp = valid;
    output.sdp.sessionIdPolicy =
        static_cast<MediaRtpSdpSessionIdPolicy>(255);
    expectInvalid(ctx, plan);
    output.sdp = valid;
    output.sdp.sessionVersionPolicy =
        static_cast<MediaRtpSdpSessionVersionPolicy>(255);
    expectInvalid(ctx, plan);
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

void testMpegTsProductionPlanningConsumesAuthoritativeEncoderPacketLayout(
    TestContext& ctx)
{
    auto planned = MediaRealtimeRtpTranscodePlanner::planPreparedInput(
        completeProductionTsRequest(), completeProductionTsStreams(),
        completeProductionTsSelection());
    EXPECT_TRUE(ctx, planned);
    if (!planned || !planned.value().avSyncRuntime) return;
    EXPECT_EQ(ctx, planned.value().avSyncRuntime->outputAdapter,
              MediaAvSyncOutputAdapterKind::ProjectMpegTs);
    EXPECT_TRUE(ctx, planned.value().videoParameters.globalHeader.has_value());
    if (planned.value().videoParameters.globalHeader) {
        EXPECT_TRUE(ctx, *planned.value().videoParameters.globalHeader);
    }
    const auto& runtime = *planned.value().avSyncRuntime;
    EXPECT_TRUE(ctx,
                std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
                    runtime.assembly.audio.duration));
    if (const auto* duration =
            std::get_if<MediaPlannedAudioSamplesDurationPlan>(
                &runtime.assembly.audio.duration)) {
        EXPECT_EQ(ctx, duration->sampleRate, 48'000);
        EXPECT_EQ(ctx, duration->samplesPerAccessUnit, std::uint32_t{1'024});
    }
    const auto* output = std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(
        &runtime.protocolOutput);
    EXPECT_TRUE(ctx, output != nullptr);
    if (output) {
        EXPECT_EQ(ctx, output->protocol.muxPlan().parameters().h264InputLayout,
                  MediaTsH264InputLayout::AnnexB);
    }
}

void testMpegTsPreparedPlanningRejectsInvalidDurationEvidence(TestContext& ctx)
{
    const auto request = completeProductionTsRequest();
    const auto streams = completeProductionTsStreams();
    const auto valid = completeProductionTsSelection();
    const auto reject = [&](MediaTsSelectedProgramPlan selected) {
        EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::planPreparedInput(
                              request, streams, selected));
    };

    auto selected = valid;
    selected.videoPacketDuration.reset();
    reject(selected);
    selected = valid;
    selected.audioPacketDuration.reset();
    reject(selected);
    selected = valid;
    selected.videoPacketDuration->packetDuration = 0;
    reject(selected);
    selected = valid;
    selected.videoPacketDuration->packetDuration = -1;
    reject(selected);
    selected = valid;
    selected.audioPacketDuration->packetDuration = 0;
    reject(selected);
    selected = valid;
    selected.audioPacketDuration->packetDuration = -1;
    reject(selected);
    selected = valid;
    selected.videoPacketDuration->streamIndex = 4;
    reject(selected);
    selected = valid;
    selected.audioPacketDuration->elementaryPid = 706;
    reject(selected);
    selected = valid;
    selected.videoPacketDuration->timeBase.num = 0;
    reject(selected);
    selected = valid;
    selected.audioPacketDuration->timeBase.den = -1;
    reject(selected);
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
    testTsDurationProbeOwnsExactEvidenceAndLosslessReplay(ctx);
    testTsDurationProbeRejectsMissingAndNonPositiveEvidence(ctx);
    testCompleteSeparateRtpAssemblyProduct(ctx);
    testSeparateRtpSdpIdentityIsPlannerOwnedAndValidated(ctx);
    testAssemblyRejectsEveryInvalidContractField(ctx);
    testMpegTsProductionPlanningConsumesAuthoritativeEncoderPacketLayout(ctx);
    testMpegTsPreparedPlanningRejectsInvalidDurationEvidence(ctx);
    testUnsupportedTopologyAndMissingSynchronizedAudioFailClosed(ctx);
}
