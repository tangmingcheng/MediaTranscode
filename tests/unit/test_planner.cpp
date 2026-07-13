#include "common/TestAssert.h"

#include "internal/graph/model/MediaCapabilityMaturity.h"
#include "internal/graph/model/MediaGraphCapability.h"
#include "internal/graph/planner/MediaGraphGpuPlanner.h"
#include "internal/graph/planner/MediaGraphDeploymentPlanner.h"
#include "internal/graph/planner/MediaGraphMeshPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/runtime/distributed/MediaGraphRemoteExecutor.h"
#include "internal/graph/runtime/gpu/MediaGpuGraphExecutor.h"
#include "internal/graph/runtime/optimizer/MediaGraphCompiler.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizer.h"
#include "internal/graph/runtime/optimizer/passes/MediaGraphIrLoweringPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaHardwarePipelineFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaNodeFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaRedundantTransferEliminationPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaSimdSchedulingPass.h"

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

FFmpegInputProgramSnapshot publicProgram(int number,
                                         int pmtPid,
                                         int pcrPid,
                                         std::initializer_list<int> streams)
{
    FFmpegInputProgramSnapshot snapshot{number, pmtPid, pcrPid, {}};
    for (const auto stream : streams) snapshot.streamBindings.push_back({stream, stream + 700});
    return snapshot;
}

MediaTsProgramInfo parserProgram(std::uint16_t number,
                                 std::uint16_t pmtPid,
                                 std::uint16_t pcrPid,
                                 std::initializer_list<std::uint16_t> pids)
{
    MediaTsProgramInfo program;
    program.programNumber = number;
    program.pmtPid = pmtPid;
    program.pcrPid = pcrPid;
    for (const auto pid : pids) program.elementaryStreams.push_back({pid, 0x06});
    return program;
}

void testTsProgramSelectorRequiresOneCrossValidatedProgram(TestContext& ctx)
{
    const std::vector<FFmpegInputProgramSnapshot> programs{
        FFmpegInputProgramSnapshot{7, 777, 701, {{5, 705}, {3, 703}}}
    };
    MediaTsProgramInventorySnapshot inventory;
    inventory.programs.push_back(parserProgram(7, 777, 701, {703, 705}));

    const auto selected = MediaTsProgramSelector::select(programs, inventory, 3, 5);
    EXPECT_TRUE(ctx, selected);
    if (!selected) return;
    EXPECT_EQ(ctx, selected.value().programNumber, 7);
    EXPECT_EQ(ctx, selected.value().programMapPid, 777);
    EXPECT_EQ(ctx, selected.value().videoPid, 703);
    EXPECT_EQ(ctx, selected.value().audioPid, 705);
    EXPECT_EQ(ctx, selected.value().pcrPid, 701);
}

void testTsProgramSelectorRejectsAmbiguityAndInventoryMismatch(TestContext& ctx)
{
    MediaTsProgramInventorySnapshot inventory;
    inventory.programs.push_back(parserProgram(7, 777, 701, {703, 705}));
    inventory.programs.push_back(parserProgram(8, 888, 801, {803, 805}));

    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {publicProgram(7, 777, 701, {3, 5}), publicProgram(8, 888, 801, {3, 5})},
        inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {publicProgram(7, 778, 701, {3, 5})}, inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {publicProgram(7, 777, 701, {3}), publicProgram(8, 888, 801, {5})},
        inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {publicProgram(7, 777, 701, {3, 5})}, inventory, 3, 9));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {FFmpegInputProgramSnapshot{7, 777, 701, {{3, 703}, {3, 705}}}},
        inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {FFmpegInputProgramSnapshot{7, 777, 701, {{3, 703}, {5, 703}}}},
        inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {FFmpegInputProgramSnapshot{7, 777, 701, {{3, 0x2000}, {5, 705}}}},
        inventory, 3, 5));
    EXPECT_FALSE(ctx, MediaTsProgramSelector::select(
        {FFmpegInputProgramSnapshot{7, 777, 701, {{3, 703}, {5, 706}}}},
        inventory, 3, 5));
}

const MediaTsSelectedProgramPlan& selectedTsProgram()
{
    static const MediaTsSelectedProgramPlan selected{7, 777, 703, 705, 701};
    return selected;
}

void testTsEvidenceCapacityCoversProbeRollbackAndPredecessor(TestContext& ctx)
{
    auto exact = MediaRealtimeTsInputPlan::create(188, 376, 188, 4);
    EXPECT_TRUE(ctx, exact);
    if (exact) EXPECT_EQ(ctx, exact.value().evidenceTimelineCapacity, std::size_t{4});
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlan::create(188, 376, 188, 3));
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlan::create(
        188, std::numeric_limits<std::uint64_t>::max(), 188,
        std::numeric_limits<std::size_t>::max()));
}

void testTsInputPlanValidatorRejectsEveryMutation(TestContext& ctx)
{
    auto created = MediaRealtimeTsInputPlan::create(188, 376, 188, 4);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    const auto valid = created.value();
    MediaRealtimeRtpInputNodePlan input{};
    input.probeSizeBytes = 376;
    input.mpegTs = valid;
    EXPECT_TRUE(ctx, MediaRealtimeTsInputPlanValidator::validate(RealtimeInputType::MpegTsUdp, input));
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlanValidator::validate(RealtimeInputType::Url, input));
    input.mpegTs.reset();
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlanValidator::validate(RealtimeInputType::MpegTsUdp, input));
    const auto mutated = [&](auto change) {
        input.mpegTs = valid;
        change(*input.mpegTs);
        EXPECT_FALSE(ctx, MediaRealtimeTsInputPlanValidator::validate(RealtimeInputType::MpegTsUdp, input));
    };
    mutated([](auto& plan) { plan.demuxFormat = "ts"; });
    mutated([](auto& plan) { plan.packetSize = 0; });
    mutated([](auto& plan) { plan.avioBufferBytes = 0; });
    mutated([](auto& plan) { plan.maximumDatagramBytes = 0; });
    mutated([](auto& plan) { plan.maximumDatagramBytes = plan.avioBufferBytes + 1; });
    mutated([](auto& plan) { plan.evidenceTimelineCapacity = 3; });
    mutated([](auto& plan) { plan.maximumPacketPositionRegressionBytes = 0; });
}

MediaRealtimeRtpTranscodeRequest avSyncRtpRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "planner-av-sync";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    return request;
}

MediaRealtimeRtpTranscodeRequest avSyncTsRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "planner-av-sync-ts";
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.parameters.execution.includeAudio = true;
    return request;
}

void testAvSyncPlannerBuildsCompleteRtpContract(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncRtpRequest());
    EXPECT_TRUE(ctx, result);
    if (!result) return;

    const MediaAvSyncPlan& plan = result.value();
    EXPECT_EQ(ctx, *plan.topology, MediaAvSyncTopology::SeparateRtpToSeparateRtp);
    EXPECT_EQ(ctx, *plan.sourceClockMode, MediaAvSyncSourceClockMode::RtpSenderReports);
    EXPECT_EQ(ctx, *plan.canonicalTimeBaseNumerator, 1);
    EXPECT_EQ(ctx, *plan.canonicalTimeBaseDenominator, 1000000000);
    EXPECT_EQ(ctx, *plan.rtp->videoInput.clockRate, 90000);
    EXPECT_EQ(ctx, *plan.rtp->audioInput.clockRate, 48000);
    EXPECT_TRUE(ctx, *plan.rtp->input.requireCommonCname);
    EXPECT_TRUE(ctx, *plan.rtp->input.requireSenderReports);
    EXPECT_EQ(ctx, *plan.rtp->input.maximumSenderClockRateErrorPpm, 1000);
    EXPECT_EQ(ctx, plan.rtp->input.maximumSenderClockResidualNs->nanoseconds(),
              250'000'000);
    EXPECT_TRUE(ctx, *plan.rtp->videoOutput.ssrc != *plan.rtp->audioOutput.ssrc);
    EXPECT_EQ(ctx, *plan.rtp->videoOutput.cname, *plan.rtp->audioOutput.cname);
    EXPECT_TRUE(ctx, *plan.rtp->output.useSharedNtpEpoch);
    EXPECT_TRUE(ctx, plan.rtp->output.senderReportIntervalNs->nanoseconds() > 0);
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validate(plan));
}

void testRawRtpInputPlannerProducesCompleteTransportPolicy(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "rtp-policy";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.rtspTransport = "udp";
    request.input.openTimeoutMs = 1'000;
    request.input.readTimeoutMs = 2'500;
    request.input.analyzeDurationUs = 100'000;
    request.input.probeSizeBytes = 32'768;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:5004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    request.parameters.execution.includeAudio = false;

    const auto raw = MediaRealtimeInputPlanner::planRawRtp(request);
    EXPECT_TRUE(ctx, raw);
    if (!raw) return;
    MediaRealtimeRtpTranscodePlan plan;
    MediaRealtimeInputPlanner::applyNodePlans(request, &raw.value(), plan);
    EXPECT_TRUE(ctx, plan.input.rtpTransport.has_value());
    if (!plan.input.rtpTransport) return;
    const MediaRealtimeRtpTransportPlan& transport = *plan.input.rtpTransport;
    EXPECT_EQ(ctx, transport.addressFamily, MediaIpAddressFamily::Ipv4);
    EXPECT_EQ(ctx, transport.bindAddress, std::string("127.0.0.1"));
    EXPECT_EQ(ctx, transport.rtpPort, static_cast<uint16_t>(5004));
    EXPECT_EQ(ctx, transport.rtcpPort, static_cast<uint16_t>(5005));
    EXPECT_EQ(ctx, transport.payloadType, static_cast<uint8_t>(96));
    EXPECT_EQ(ctx, transport.clockRate, 90'000);
    EXPECT_EQ(ctx, transport.receiveBufferBytes, 4 * 1024 * 1024);
    EXPECT_EQ(ctx, transport.maximumDatagramBytes, 65'535);
    EXPECT_EQ(ctx, transport.reorderWindowPackets, static_cast<std::size_t>(64));
    EXPECT_EQ(ctx, transport.maximumReorderDelayMs, 100);
    EXPECT_EQ(ctx, transport.cancellableReadTimeoutMs, 2'500);
    EXPECT_TRUE(ctx, transport.requireSenderReports);
    EXPECT_TRUE(ctx, transport.requireCname);
    EXPECT_EQ(ctx, transport.senderReportTimeoutMs, 3'000);
    EXPECT_EQ(ctx, transport.cnameTimeoutMs, 5'000);
    EXPECT_EQ(ctx, transport.rtcpCompositionMode, MediaRtcpCompositionMode::StrictCompoundRfc3550);

    auto ipv6Request = request;
    ipv6Request.input.videoRtp.url = "rtp://[::1]:5004";
    const auto ipv6Raw = MediaRealtimeInputPlanner::planRawRtp(ipv6Request);
    EXPECT_TRUE(ctx, ipv6Raw);
    if (ipv6Raw) {
        EXPECT_EQ(ctx, ipv6Raw.value().videoTransport.addressFamily, MediaIpAddressFamily::Ipv6);
        EXPECT_EQ(ctx, ipv6Raw.value().videoTransport.bindAddress, std::string("::1"));
        EXPECT_TRUE(ctx, ipv6Raw.value().videoSdp.empty());
        EXPECT_EQ(ctx, ipv6Raw.value().videoDepacketizer.codecName, std::string("h264"));
    }

    auto multicastV4 = request;
    multicastV4.input.videoRtp.url = "rtp://239.1.2.3:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(multicastV4));
    auto zeroNetwork = request;
    zeroNetwork.input.videoRtp.url = "rtp://0.1.2.3:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(zeroNetwork));
    auto reservedHigh = request;
    reservedHigh.input.videoRtp.url = "rtp://240.0.0.1:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(reservedHigh));
    auto reserved255 = request;
    reserved255.input.videoRtp.url = "rtp://255.0.0.1:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(reserved255));
    auto loopback = request;
    loopback.input.videoRtp.url = "rtp://127.0.0.1:5004";
    EXPECT_TRUE(ctx, MediaRealtimeInputPlanner::planRawRtp(loopback));
    auto multicastV6 = request;
    multicastV6.input.videoRtp.url = "rtp://[ff02::1]:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(multicastV6));
    auto hostname = request;
    hostname.input.videoRtp.url = "rtp://localhost:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(hostname));
    auto familyMismatch = request;
    familyMismatch.input.videoRtp.url = "rtp://[127.0.0.1]:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(familyMismatch));
}

void testAvSyncPlannerBuildsCompleteTsContract(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncTsRequest(), &selectedTsProgram());
    EXPECT_TRUE(ctx, result);
    if (!result) return;

    const MediaAvSyncPlan& plan = result.value();
    EXPECT_EQ(ctx, *plan.topology, MediaAvSyncTopology::MpegTsToMpegTs);
    EXPECT_EQ(ctx, *plan.sourceClockMode, MediaAvSyncSourceClockMode::MpegTsPcr);
    EXPECT_TRUE(ctx, plan.ts.has_value());
    EXPECT_EQ(ctx, *plan.ts->programNumber, 7);
    EXPECT_EQ(ctx, *plan.ts->programMapPid, 777);
    EXPECT_EQ(ctx, *plan.ts->videoPid, 703);
    EXPECT_EQ(ctx, *plan.ts->audioPid, 705);
    EXPECT_EQ(ctx, *plan.ts->pcrPid, 701);
    EXPECT_TRUE(ctx, plan.ts->pcrIntervalNs->nanoseconds() > 0);
    EXPECT_TRUE(ctx, *plan.ts->maximumPcrGapNs >= *plan.ts->pcrIntervalNs);
    EXPECT_EQ(ctx, *plan.ts->timestampTimeBaseNumerator, 1);
    EXPECT_EQ(ctx, *plan.ts->timestampTimeBaseDenominator, 90000);
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validate(plan));
}

void testAvSyncPlannerRejectsSeparateRtpToTs(TestContext& ctx)
{
    auto request = avSyncRtpRequest();
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    const auto result = MediaAvSyncPlanner::plan(request);
    EXPECT_FALSE(ctx, result);
    if (!result) EXPECT_EQ(ctx, result.error().code, media::ErrorCode::Unsupported);
}

void testAvSyncValidatorRejectsMissingAndInconsistentFields(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncRtpRequest());
    EXPECT_TRUE(ctx, result);
    if (!result) return;
    const MediaAvSyncPlan complete = result.value();

    auto expectInvalid = [&ctx](MediaAvSyncPlan plan) {
        const auto status = MediaAvSyncPlanValidator::validate(plan);
        EXPECT_FALSE(ctx, status);
        if (!status) EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    };

#define EXPECT_MISSING(field)                 \
    do {                                      \
        MediaAvSyncPlan missing = complete;   \
        missing.field.reset();                \
        expectInvalid(std::move(missing));    \
    } while (false)

    EXPECT_MISSING(topology);
    EXPECT_MISSING(sourceClockMode);
    EXPECT_MISSING(masterClockMode);
    EXPECT_MISSING(canonicalTimeBaseNumerator);
    EXPECT_MISSING(canonicalTimeBaseDenominator);
    EXPECT_MISSING(startup.requireVideoKeyFrame);
    EXPECT_MISSING(startup.trimAudioToCommonStart);
    EXPECT_MISSING(startup.maximumWaitNs);
    EXPECT_MISSING(startup.prerollNs);
    EXPECT_MISSING(startup.keyFrameWaitNs);
    EXPECT_MISSING(startup.maximumAudioTrimNs);
    EXPECT_MISSING(startup.maximumInitialSkewNs);
    EXPECT_MISSING(startup.outputLeadNs);
    EXPECT_MISSING(audioServo.deadbandNs);
    EXPECT_MISSING(audioServo.shortControlWindowNs);
    EXPECT_MISSING(audioServo.longControlWindowNs);
    EXPECT_MISSING(audioServo.proportionalGainPpm);
    EXPECT_MISSING(audioServo.integralGainPpm);
    EXPECT_MISSING(audioServo.maximumSlewPpmPerSecond);
    EXPECT_MISSING(audioServo.normalCorrectionLimitPpm);
    EXPECT_MISSING(audioServo.recoveryCorrectionLimitPpm);
    EXPECT_MISSING(audioServo.compensationWindowNs);
    EXPECT_MISSING(video.earlyHoldThresholdNs);
    EXPECT_MISSING(video.lateDisplayThresholdNs);
    EXPECT_MISSING(video.dropThresholdNs);
    EXPECT_MISSING(video.allowRecoveryRepeat);
    EXPECT_MISSING(video.maximumConsecutiveRecoveryActions);
    EXPECT_MISSING(recovery.suspectThresholdNs);
    EXPECT_MISSING(recovery.hardDiscontinuityThresholdNs);
    EXPECT_MISSING(recovery.reacquisitionTimeoutNs);
    EXPECT_MISSING(metrics.collectStateAndGeneration);
    EXPECT_MISSING(metrics.collectClockEvidence);
    EXPECT_MISSING(metrics.collectQueueDurations);
    EXPECT_MISSING(metrics.collectPhaseErrors);
    EXPECT_MISSING(metrics.collectAudioCorrection);
    EXPECT_MISSING(metrics.collectVideoRecoveryCounts);
    EXPECT_MISSING(metrics.collectDiscontinuityCounts);
    EXPECT_MISSING(metrics.collectProtocolClockHealth);
    EXPECT_MISSING(metrics.maximumStartupSkewNs);
    EXPECT_MISSING(metrics.maximumSteadyP95SkewNs);
    EXPECT_MISSING(metrics.maximumSteadyP99SkewNs);
    EXPECT_MISSING(metrics.maximumDriftNsPerHour);
    EXPECT_MISSING(rtp->videoInput.identity);
    EXPECT_MISSING(rtp->videoInput.payloadType);
    EXPECT_MISSING(rtp->videoInput.clockRate);
    EXPECT_MISSING(rtp->audioInput.identity);
    EXPECT_MISSING(rtp->audioInput.payloadType);
    EXPECT_MISSING(rtp->audioInput.clockRate);
    EXPECT_MISSING(rtp->input.requireCommonCname);
    EXPECT_MISSING(rtp->input.requireSenderReports);
    EXPECT_MISSING(rtp->input.senderReportTimeoutNs);
    EXPECT_MISSING(rtp->input.maximumExtrapolationNs);
    EXPECT_MISSING(rtp->input.maximumSenderReportSkewNs);
    EXPECT_MISSING(rtp->input.maximumSenderClockRateErrorPpm);
    EXPECT_MISSING(rtp->input.maximumSenderClockResidualNs);
    EXPECT_MISSING(rtp->videoOutput.identity);
    EXPECT_MISSING(rtp->videoOutput.payloadType);
    EXPECT_MISSING(rtp->videoOutput.clockRate);
    EXPECT_MISSING(rtp->videoOutput.ssrc);
    EXPECT_MISSING(rtp->videoOutput.baseTimestamp);
    EXPECT_MISSING(rtp->videoOutput.cname);
    EXPECT_MISSING(rtp->audioOutput.identity);
    EXPECT_MISSING(rtp->audioOutput.payloadType);
    EXPECT_MISSING(rtp->audioOutput.clockRate);
    EXPECT_MISSING(rtp->audioOutput.ssrc);
    EXPECT_MISSING(rtp->audioOutput.baseTimestamp);
    EXPECT_MISSING(rtp->audioOutput.cname);
    EXPECT_MISSING(rtp->output.useSharedNtpEpoch);
    EXPECT_MISSING(rtp->output.senderReportIntervalNs);
#undef EXPECT_MISSING
    MediaAvSyncPlan missingRtpPolicy = complete;
    missingRtpPolicy.rtp.reset();
    expectInvalid(std::move(missingRtpPolicy));

    MediaAvSyncPlan unordered = complete;
    unordered.video.lateDisplayThresholdNs = *unordered.recovery.hardDiscontinuityThresholdNs;
    expectInvalid(std::move(unordered));
    MediaAvSyncPlan excessiveNormalCorrection = complete;
    excessiveNormalCorrection.audioServo.normalCorrectionLimitPpm = 1001;
    expectInvalid(std::move(excessiveNormalCorrection));
    MediaAvSyncPlan excessiveRecoveryCorrection = complete;
    excessiveRecoveryCorrection.audioServo.recoveryCorrectionLimitPpm = 5001;
    expectInvalid(std::move(excessiveRecoveryCorrection));

    const auto tsResult = MediaAvSyncPlanner::plan(avSyncTsRequest(), &selectedTsProgram());
    EXPECT_TRUE(ctx, tsResult);
    if (!tsResult) return;
    const MediaAvSyncPlan completeTs = tsResult.value();
#define EXPECT_MISSING_TS(field)                 \
    do {                                         \
        MediaAvSyncPlan missing = completeTs;    \
        missing.ts->field.reset();               \
        expectInvalid(std::move(missing));       \
    } while (false)
    EXPECT_MISSING_TS(programNumber);
    EXPECT_MISSING_TS(programMapPid);
    EXPECT_MISSING_TS(videoPid);
    EXPECT_MISSING_TS(audioPid);
    EXPECT_MISSING_TS(pcrPid);
    EXPECT_MISSING_TS(pcrIntervalNs);
    EXPECT_MISSING_TS(maximumPcrGapNs);
    EXPECT_MISSING_TS(maximumPcrJitterNs);
    EXPECT_MISSING_TS(timestampTimeBaseNumerator);
    EXPECT_MISSING_TS(timestampTimeBaseDenominator);
#undef EXPECT_MISSING_TS
    MediaAvSyncPlan missingTsPolicy = completeTs;
    missingTsPolicy.ts.reset();
    expectInvalid(std::move(missingTsPolicy));
}

MediaRunningTime avSyncTime(std::int64_t nanoseconds)
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

void expectInvalidAvSyncMutation(TestContext& ctx,
                                 MediaAvSyncPlan plan,
                                 const char* invariant)
{
    const auto status = MediaAvSyncPlanValidator::validate(plan);
    if (status) std::cerr << "expected invalid A/V sync invariant: " << invariant << '\n';
    EXPECT_FALSE(ctx, status);
}

void testAvSyncValidatorRejectsProtocolIdentifierBoundaries(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncTsRequest(), &selectedTsProgram());
    EXPECT_TRUE(ctx, result);
    if (!result) return;
    const MediaAvSyncPlan complete = result.value();
    auto mutate = [&](const char* name, auto mutation) {
        MediaAvSyncPlan plan = complete;
        mutation(*plan.ts);
        expectInvalidAvSyncMutation(ctx, std::move(plan), name);
    };

    mutate("program number zero", [](auto& ts) { ts.programNumber = 0; });
    mutate("program number overflow", [](auto& ts) { ts.programNumber = 0x10000; });
    mutate("PMT PID reserved low", [](auto& ts) { ts.programMapPid = 0x000F; });
    mutate("video PID reserved low", [](auto& ts) { ts.videoPid = 0x0010; });
    mutate("audio PID null", [](auto& ts) { ts.audioPid = 0x1FFF; });
    mutate("PCR PID overflow", [](auto& ts) { ts.pcrPid = 0x2000; });
}

void testAvSyncValidatorRejectsIsolatedNumericAndOrderingInvariants(TestContext& ctx)
{
    const auto rtpResult = MediaAvSyncPlanner::plan(avSyncRtpRequest());
    const auto tsResult = MediaAvSyncPlanner::plan(avSyncTsRequest(), &selectedTsProgram());
    EXPECT_TRUE(ctx, rtpResult);
    EXPECT_TRUE(ctx, tsResult);
    if (!rtpResult || !tsResult) return;

    const MediaAvSyncPlan rtp = rtpResult.value();
    auto rejectRtp = [&](const char* name, auto mutation) {
        MediaAvSyncPlan plan = rtp;
        mutation(plan);
        expectInvalidAvSyncMutation(ctx, std::move(plan), name);
    };
    rejectRtp("startup wait positive", [](auto& p) { p.startup.maximumWaitNs = avSyncTime(0); });
    rejectRtp("startup preroll positive", [](auto& p) { p.startup.prerollNs = avSyncTime(-1); });
    rejectRtp("startup skew within output lead", [](auto& p) { p.startup.maximumInitialSkewNs = *p.startup.outputLeadNs; });
    rejectRtp("audio deadband positive", [](auto& p) { p.audioServo.deadbandNs = avSyncTime(0); });
    rejectRtp("audio filter after deadband", [](auto& p) { p.audioServo.shortControlWindowNs = *p.audioServo.deadbandNs; });
    rejectRtp("audio estimator after filter", [](auto& p) { p.audioServo.longControlWindowNs = *p.audioServo.shortControlWindowNs; });
    rejectRtp("audio control after filter", [](auto& p) { p.audioServo.compensationWindowNs = *p.audioServo.shortControlWindowNs; });
    rejectRtp("audio estimator after control", [](auto& p) { p.audioServo.longControlWindowNs = *p.audioServo.compensationWindowNs; });
    rejectRtp("audio recovery not below normal", [](auto& p) { p.audioServo.recoveryCorrectionLimitPpm = *p.audioServo.normalCorrectionLimitPpm - 1; });
    rejectRtp("audio slew within normal correction", [](auto& p) { p.audioServo.maximumSlewPpmPerSecond = *p.audioServo.normalCorrectionLimitPpm + 1; });
    rejectRtp("audio hard threshold after recovery", [](auto& p) { p.recovery.hardDiscontinuityThresholdNs = *p.recovery.suspectThresholdNs; });
    rejectRtp("video hold positive", [](auto& p) { p.video.earlyHoldThresholdNs = avSyncTime(0); });
    rejectRtp("video late after hold", [](auto& p) { p.video.lateDisplayThresholdNs = *p.video.earlyHoldThresholdNs; });
    rejectRtp("video drop after late", [](auto& p) { p.video.dropThresholdNs = *p.video.lateDisplayThresholdNs; });
    rejectRtp("video recovery count positive", [](auto& p) { p.video.maximumConsecutiveRecoveryActions = 0; });
    rejectRtp("discontinuity after video recovery", [](auto& p) { p.recovery.suspectThresholdNs = *p.video.dropThresholdNs; });
    rejectRtp("reacquire after hard threshold", [](auto& p) { p.recovery.reacquisitionTimeoutNs = *p.recovery.hardDiscontinuityThresholdNs; });
    rejectRtp("metrics p95 positive", [](auto& p) { p.metrics.maximumSteadyP95SkewNs = avSyncTime(0); });
    rejectRtp("metrics p99 after p95", [](auto& p) { p.metrics.maximumSteadyP99SkewNs = avSyncTime(1); });
    rejectRtp("metrics startup accepts p99", [](auto& p) { p.metrics.maximumStartupSkewNs = avSyncTime(1); });
    rejectRtp("RTP SR interval positive", [](auto& p) { p.rtp->output.senderReportIntervalNs = avSyncTime(0); });
    rejectRtp("RTP SR interval before timeout", [](auto& p) { p.rtp->output.senderReportIntervalNs = *p.rtp->input.senderReportTimeoutNs; });
    rejectRtp("RTP timeout within extrapolation", [](auto& p) { p.rtp->input.senderReportTimeoutNs = *p.rtp->input.maximumExtrapolationNs; });
    rejectRtp("RTP extrapolation before reacquire", [](auto& p) { p.rtp->input.maximumExtrapolationNs = *p.recovery.reacquisitionTimeoutNs; });
    rejectRtp("RTP SR skew positive", [](auto& p) { p.rtp->input.maximumSenderReportSkewNs = avSyncTime(0); });
    rejectRtp("RTP SR skew below hard discontinuity", [](auto& p) { p.rtp->input.maximumSenderReportSkewNs = *p.recovery.hardDiscontinuityThresholdNs; });
    rejectRtp("RTP sender clock rate error positive", [](auto& p) { p.rtp->input.maximumSenderClockRateErrorPpm = 0; });
    rejectRtp("RTP sender clock residual positive", [](auto& p) { p.rtp->input.maximumSenderClockResidualNs = avSyncTime(0); });

    const MediaAvSyncPlan ts = tsResult.value();
    auto rejectTs = [&](const char* name, auto mutation) {
        MediaAvSyncPlan plan = ts;
        mutation(plan);
        expectInvalidAvSyncMutation(ctx, std::move(plan), name);
    };
    rejectTs("TS PCR interval positive", [](auto& p) { p.ts->pcrIntervalNs = avSyncTime(0); });
    rejectTs("TS PCR jitter below interval", [](auto& p) { p.ts->maximumPcrJitterNs = *p.ts->pcrIntervalNs; });
    rejectTs("TS PCR gap after interval", [](auto& p) { p.ts->maximumPcrGapNs = *p.ts->pcrIntervalNs; });
}

} // namespace

int main()
{
    TestContext ctx;

    testTsInputPlanValidatorRejectsEveryMutation(ctx);
    testTsProgramSelectorRequiresOneCrossValidatedProgram(ctx);
    testTsProgramSelectorRejectsAmbiguityAndInventoryMismatch(ctx);
    testTsEvidenceCapacityCoversProbeRollbackAndPredecessor(ctx);
    testAvSyncPlannerBuildsCompleteRtpContract(ctx);
    testRawRtpInputPlannerProducesCompleteTransportPolicy(ctx);
    testAvSyncPlannerBuildsCompleteTsContract(ctx);
    testAvSyncPlannerRejectsSeparateRtpToTs(ctx);
    testAvSyncValidatorRejectsMissingAndInconsistentFields(ctx);
    testAvSyncValidatorRejectsProtocolIdentifierBoundaries(ctx);
    testAvSyncValidatorRejectsIsolatedNumericAndOrderingInvariants(ctx);
    MediaInputAudioStreamInfo source;
    source.streamIndex = 0;
    source.codecName = "aac";
    source.sampleRate = 48000;
    source.channels = 2;
    source.bitrateBitsPerSecond = 320000;

    MediaAudioPipelinePlannerOptions copyOptions(true);
    copyOptions.requestedCodecName = "aac";
    copyOptions.requestedSampleRate = 48000;
    copyOptions.requestedChannels = 2;
    copyOptions.requestedBitrateKbps = 320;
    const auto copy = MediaAudioPipelinePlanner::planKnownAudioTranscode(source, copyOptions);
    EXPECT_TRUE(ctx, copy);
    if (copy) EXPECT_EQ(ctx, copy.value().branchMode, MediaBranchMode::CopyPacket);

    auto transcodeOptions = copyOptions;
    transcodeOptions.requestedSampleRate = 44100;
    const auto transcode = MediaAudioPipelinePlanner::planKnownAudioTranscode(source, transcodeOptions);
    EXPECT_TRUE(ctx, transcode);
    if (transcode) EXPECT_EQ(ctx, transcode.value().branchMode, MediaBranchMode::TranscodeFrame);

    const MediaGraphPlanningPolicy defaultPlanningPolicy;
    EXPECT_FALSE(ctx, defaultPlanningPolicy.enableDistributedExecution);
    EXPECT_FALSE(ctx, defaultPlanningPolicy.enableGpuPlanning);
    EXPECT_FALSE(ctx, defaultPlanningPolicy.enableMeshPlanning);
    EXPECT_EQ(ctx, defaultPlanningPolicy.optimizationPolicy.level, MediaGraphOptimizationLevel::None);
    const MediaRealtimePlannerOptions defaultRealtimeOptions;
    EXPECT_EQ(ctx, defaultRealtimeOptions.basePort, 0);
    EXPECT_EQ(ctx, defaultRealtimeOptions.targetLatencyUs, static_cast<int64_t>(0));
    EXPECT_FALSE(ctx, defaultRealtimeOptions.enableGpuPlanning);
    EXPECT_FALSE(ctx, defaultRealtimeOptions.enableMeshPlanning);
    EXPECT_FALSE(ctx, defaultRealtimeOptions.preferZeroCopy);
    EXPECT_FALSE(ctx, defaultRealtimeOptions.enableNodeFusion);

    MediaRealtimePlannerOptions realtimeOptions;
    realtimeOptions.edgeNodeId = "edge";
    realtimeOptions.workerNodeId = "worker";
    realtimeOptions.host = "127.0.0.1";
    realtimeOptions.zone = "local";
    realtimeOptions.basePort = 20000;
    realtimeOptions.targetLatencyUs = 10000;
    realtimeOptions.maxLatencyUs = 20000;
    MediaGraph realtimeGraph;
    realtimeGraph.addNode(MediaNodeKind::RealtimeInput, "realtime.unsupported");
    const auto realtimePlan = MediaRealtimePlanner::plan(realtimeGraph, realtimeOptions);
    EXPECT_FALSE(ctx, realtimePlan);
    if (!realtimePlan) EXPECT_EQ(ctx, realtimePlan.error().code, media::ErrorCode::Unsupported);

    realtimeOptions.enableNodeFusion = true;
    const auto fusionPolicy = MediaRealtimePlanner::buildPolicy(realtimeOptions);
    EXPECT_TRUE(ctx, fusionPolicy.optimizationPolicy.enableNodeFusion);
    EXPECT_TRUE(ctx, fusionPolicy.optimizationPolicy.enabled());
    MediaGraph fusionGraph;
    fusionGraph.addNode(MediaNodeKind::VideoFilter, "realtime.fusion.unsupported");
    MediaGraphCompiler fusionCompiler;
    const auto fusionCompile = fusionCompiler.compile(
        std::move(fusionGraph), fusionPolicy.optimizationPolicy);
    EXPECT_FALSE(ctx, fusionCompile);
    if (!fusionCompile) EXPECT_EQ(ctx, fusionCompile.error().code, media::ErrorCode::Unsupported);

    EXPECT_EQ(ctx, MediaGpuGraphExecutor::capabilityMaturity(), MediaCapabilityMaturity::Unsupported);
    EXPECT_EQ(ctx, MediaGraphRemoteExecutor::capabilityMaturity(), MediaCapabilityMaturity::Unsupported);
    EXPECT_EQ(ctx, MediaGraphOptimizer::capabilityMaturity(), MediaCapabilityMaturity::Unsupported);
    EXPECT_EQ(ctx, mediaGraphCapabilityMaturity(MediaGraphCapability::CudaNvencTranscode),
              MediaCapabilityMaturity::Stable);
    EXPECT_EQ(ctx, mediaGraphCapabilityMaturity(MediaGraphCapability::GraphOptimization),
              MediaCapabilityMaturity::Unsupported);
    EXPECT_EQ(ctx, mediaGraphCapabilityMaturity(MediaGraphCapability::GenericGpuExecution),
              MediaCapabilityMaturity::Unsupported);
    EXPECT_EQ(ctx, mediaGraphCapabilityMaturity(MediaGraphCapability::DistributedExecution),
              MediaCapabilityMaturity::Unsupported);

    MediaGraph gpuGraph;
    gpuGraph.addNode(MediaNodeKind::VideoEncode, "gpu.boundary");
    MediaGraphPlanningPolicy gpuPolicy;
    gpuPolicy.enableGpuPlanning = true;
    const auto gpuPlan = MediaGraphGpuPlanner::plan(gpuGraph, gpuPolicy);
    EXPECT_FALSE(ctx, gpuPlan);
    if (!gpuPlan) EXPECT_EQ(ctx, gpuPlan.error().code, media::ErrorCode::Unsupported);

    MediaGraphClusterTopology multiNodeTopology;
    MediaGraphClusterNode firstNode;
    firstNode.address.nodeId = "first";
    firstNode.address.host = "127.0.0.1";
    firstNode.address.port = 19000;
    firstNode.available = true;
    multiNodeTopology.addNode(firstNode);
    MediaGraphClusterNode secondNode;
    secondNode.address.nodeId = "second";
    secondNode.address.host = "127.0.0.1";
    secondNode.address.port = 19001;
    secondNode.available = true;
    multiNodeTopology.addNode(secondNode);
    MediaGraphPlanningPolicy localOnlyPolicy;
    const auto implicitDistributed = MediaGraphDeploymentPlanner::plan(
        gpuGraph, multiNodeTopology, localOnlyPolicy);
    EXPECT_FALSE(ctx, implicitDistributed);
    if (!implicitDistributed) {
        EXPECT_EQ(ctx, implicitDistributed.error().code, media::ErrorCode::Unsupported);
    }

    MediaGraphPlanningPolicy explicitDistributedPolicy;
    explicitDistributedPolicy.enableDistributedExecution = true;
    const auto explicitDistributed = MediaGraphDeploymentPlanner::plan(
        gpuGraph, multiNodeTopology, explicitDistributedPolicy);
    EXPECT_FALSE(ctx, explicitDistributed);
    if (!explicitDistributed) {
        EXPECT_EQ(ctx, explicitDistributed.error().code, media::ErrorCode::Unsupported);
    }

    MediaGraphClusterTopology singleNodeTopology;
    singleNodeTopology.addNode(firstNode);
    const auto topLevelDistributed = MediaGraphPlanner::plan(
        gpuGraph, singleNodeTopology, explicitDistributedPolicy);
    EXPECT_FALSE(ctx, topLevelDistributed);
    if (!topLevelDistributed) {
        EXPECT_EQ(ctx, topLevelDistributed.error().code, media::ErrorCode::Unsupported);
    }

    MediaGraphPlanningPolicy meshPolicy;
    meshPolicy.enableMeshPlanning = true;
    const auto topLevelMesh = MediaGraphPlanner::plan(gpuGraph, singleNodeTopology, meshPolicy);
    EXPECT_FALSE(ctx, topLevelMesh);
    if (!topLevelMesh) EXPECT_EQ(ctx, topLevelMesh.error().code, media::ErrorCode::Unsupported);

    MediaGpuGraphCommandList commands;
    MediaGpuGraphCommand command;
    command.kind = MediaGpuGraphCommandKind::Kernel;
    command.nodeId = gpuGraph.nodes().front().id;
    commands.commands.push_back(command);
    MediaGpuGraphExecutor gpuExecutor;
    const auto prepareGpu = gpuExecutor.prepare(std::move(commands));
    EXPECT_FALSE(ctx, prepareGpu);
    if (!prepareGpu) EXPECT_EQ(ctx, prepareGpu.error().code, media::ErrorCode::Unsupported);
    EXPECT_EQ(ctx, gpuExecutor.state(), MediaGpuGraphExecutorState::Idle);
    const auto executeGpu = gpuExecutor.execute();
    EXPECT_FALSE(ctx, executeGpu);
    if (!executeGpu) EXPECT_EQ(ctx, executeGpu.error().code, media::ErrorCode::Unsupported);

    MediaGraphDeploymentPlan deployment;
    MediaGraphNodeAddress address;
    address.nodeId = "remote";
    address.host = "127.0.0.1";
    address.port = 19000;
    deployment.assign(gpuGraph.nodes().front().id, address);
    MediaGraphRemoteExecutor remoteExecutor;
    const auto deployRemote = remoteExecutor.deploy(std::move(deployment));
    EXPECT_FALSE(ctx, deployRemote);
    if (!deployRemote) EXPECT_EQ(ctx, deployRemote.error().code, media::ErrorCode::Unsupported);
    EXPECT_EQ(ctx, remoteExecutor.state(), MediaGraphRemoteExecutorState::Idle);
    const auto startRemote = remoteExecutor.start();
    EXPECT_FALSE(ctx, startRemote);
    if (!startRemote) EXPECT_EQ(ctx, startRemote.error().code, media::ErrorCode::Unsupported);
    const auto stopRemote = remoteExecutor.stop();
    EXPECT_FALSE(ctx, stopRemote);
    if (!stopRemote) EXPECT_EQ(ctx, stopRemote.error().code, media::ErrorCode::Unsupported);

    MediaGraphOptimizationPolicy unsupportedOptimization;
    unsupportedOptimization.level = MediaGraphOptimizationLevel::Aggressive;
    MediaGraphCompiler compiler;
    const auto compiled = compiler.compile(std::move(gpuGraph), unsupportedOptimization);
    EXPECT_FALSE(ctx, compiled);
    if (!compiled) EXPECT_EQ(ctx, compiled.error().code, media::ErrorCode::Unsupported);

    MediaGraph passGraph;
    passGraph.addNode(MediaNodeKind::VideoFilter, "optimizer.boundary");
    MediaGraphOptimizationReport report;
    MediaGraphOptimizationPolicy passPolicy;
    passPolicy.level = MediaGraphOptimizationLevel::Aggressive;
    passPolicy.enableNodeFusion = true;
    passPolicy.enableRedundantTransferElimination = true;
    passPolicy.zeroCopyPolicy.mode = MediaZeroCopyMode::Required;

    MediaGraphIrLoweringPass irLowering;
    const auto irStatus = irLowering.run(passGraph, passPolicy, report);
    EXPECT_FALSE(ctx, irStatus);
    if (!irStatus) EXPECT_EQ(ctx, irStatus.error().code, media::ErrorCode::Unsupported);

    MediaNodeFusionPass nodeFusion;
    const auto fusionStatus = nodeFusion.run(passGraph, passPolicy, report);
    EXPECT_FALSE(ctx, fusionStatus);
    if (!fusionStatus) EXPECT_EQ(ctx, fusionStatus.error().code, media::ErrorCode::Unsupported);

    MediaRedundantTransferEliminationPass transferElimination;
    const auto transferStatus = transferElimination.run(passGraph, passPolicy, report);
    EXPECT_FALSE(ctx, transferStatus);
    if (!transferStatus) EXPECT_EQ(ctx, transferStatus.error().code, media::ErrorCode::Unsupported);

    MediaHardwarePipelineFusionPass hardwareFusion;
    const auto hardwareFusionStatus = hardwareFusion.run(passGraph, passPolicy, report);
    EXPECT_FALSE(ctx, hardwareFusionStatus);
    if (!hardwareFusionStatus) EXPECT_EQ(ctx, hardwareFusionStatus.error().code, media::ErrorCode::Unsupported);

    MediaSimdSchedulingPass simdScheduling;
    const auto simdStatus = simdScheduling.run(passGraph, passPolicy, report);
    EXPECT_FALSE(ctx, simdStatus);
    if (!simdStatus) EXPECT_EQ(ctx, simdStatus.error().code, media::ErrorCode::Unsupported);
    return ctx.failures == 0 ? 0 : 1;
}
