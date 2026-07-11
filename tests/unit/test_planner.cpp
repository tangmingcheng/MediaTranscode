#include "common/TestAssert.h"

#include "internal/graph/model/MediaCapabilityMaturity.h"
#include "internal/graph/model/MediaGraphCapability.h"
#include "internal/graph/planner/MediaGraphGpuPlanner.h"
#include "internal/graph/planner/MediaGraphDeploymentPlanner.h"
#include "internal/graph/planner/MediaGraphMeshPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
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

int main()
{
    TestContext ctx;
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
