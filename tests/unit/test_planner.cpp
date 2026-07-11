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
    EXPECT_TRUE(ctx, *plan.rtp->videoOutput.ssrc != *plan.rtp->audioOutput.ssrc);
    EXPECT_EQ(ctx, *plan.rtp->videoOutput.cname, *plan.rtp->audioOutput.cname);
    EXPECT_TRUE(ctx, *plan.rtp->output.useSharedNtpEpoch);
    EXPECT_TRUE(ctx, plan.rtp->output.senderReportIntervalNs->nanoseconds() > 0);
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validate(plan));
}

void testAvSyncPlannerBuildsCompleteTsContract(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncTsRequest());
    EXPECT_TRUE(ctx, result);
    if (!result) return;

    const MediaAvSyncPlan& plan = result.value();
    EXPECT_EQ(ctx, *plan.topology, MediaAvSyncTopology::MpegTsToMpegTs);
    EXPECT_EQ(ctx, *plan.sourceClockMode, MediaAvSyncSourceClockMode::MpegTsPcr);
    EXPECT_TRUE(ctx, plan.ts.has_value());
    EXPECT_TRUE(ctx, *plan.ts->programNumber > 0);
    EXPECT_TRUE(ctx, *plan.ts->videoPid != *plan.ts->audioPid);
    EXPECT_TRUE(ctx, *plan.ts->pcrPid == *plan.ts->videoPid);
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

    const auto tsResult = MediaAvSyncPlanner::plan(avSyncTsRequest());
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

} // namespace

int main()
{
    TestContext ctx;
    testAvSyncPlannerBuildsCompleteRtpContract(ctx);
    testAvSyncPlannerBuildsCompleteTsContract(ctx);
    testAvSyncPlannerRejectsSeparateRtpToTs(ctx);
    testAvSyncValidatorRejectsMissingAndInconsistentFields(ctx);
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
