#include "common/TestAssert.h"

#include "internal/graph/model/MediaCapabilityMaturity.h"
#include "internal/graph/model/MediaGraphCapability.h"
#include "internal/graph/planner/MediaGraphGpuPlanner.h"
#include "internal/graph/planner/MediaGraphDeploymentPlanner.h"
#include "internal/graph/planner/MediaGraphMeshPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/planner/MediaPipelineScorer.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaSelectedEncoderPacketLayoutResolver.h"
#include "internal/graph/planner/capability/MediaVideoCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/audio/MediaAudioProfile.h"
#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"
#include "internal/graph/planner/audio/capability/MediaAudioEncoderTargetIdentityValidator.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/distributed/MediaGraphRemoteExecutor.h"
#include "internal/graph/runtime/gpu/MediaGpuGraphExecutor.h"
#include "internal/graph/runtime/optimizer/MediaGraphCompiler.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizer.h"
#include "internal/graph/runtime/optimizer/passes/MediaGraphIrLoweringPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaHardwarePipelineFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaNodeFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaRedundantTransferEliminationPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaSimdSchedulingPass.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <algorithm>
#include <string_view>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

void runAvSyncProductionPlanTests(TestContext& ctx);

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
    auto exact = MediaRealtimeTsInputPlan::create(188, 3'008, 188, 35, 1);
    EXPECT_TRUE(ctx, exact);
    if (exact) EXPECT_EQ(ctx, exact.value().evidenceTimelineCapacity, std::size_t{35});
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlan::create(188, 3'008, 188, 34, 1));
    EXPECT_FALSE(ctx, MediaRealtimeTsInputPlan::create(
        188, std::numeric_limits<std::uint64_t>::max(), 188,
        std::numeric_limits<std::size_t>::max(), 1));
}

void testTsInputPlanValidatorRejectsEveryMutation(TestContext& ctx)
{
    auto created = MediaRealtimeTsInputPlan::create(188, 3'008, 188, 35, 1);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto valid = created.value();
    valid.programNumber = 7;
    valid.programMapPid = 777;
    valid.videoPid = 703;
    valid.audioPid = 705;
    valid.pcrPid = 701;
    valid.maximumPcrGap27Mhz = 2'700'000;
    valid.projectionCapacity = valid.evidenceTimelineCapacity;
    valid.initialAcquiringVideoPacketCapacity = 8;
    valid.initialAcquiringAudioPacketCapacity = 8;
    valid.initialAcquiringVideoByteCapacity = 8 * 1024 * 1024;
    valid.initialAcquiringAudioByteCapacity = 1024 * 1024;
    valid.maximumAcquiringVideoPacketBytes = 1024 * 1024;
    valid.maximumAcquiringAudioPacketBytes = 128 * 1024;
    valid.timestampTimeBaseNumerator = 1;
    valid.timestampTimeBaseDenominator = 90'000;
    MediaRealtimeRtpInputNodePlan input{};
    input.probeSizeBytes = 3'008;
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
    mutated([](auto& plan) { plan.evidenceTimelineCapacity = 34; });
    mutated([](auto& plan) { plan.maximumPacketPositionRegressionBytes = 0; });
    mutated([](auto& plan) { plan.pesProvenanceCapacity = 0; });
    mutated([](auto& plan) { plan.maximumPcrGap27Mhz = 0; });
    mutated([](auto& plan) { plan.initialAcquiringVideoPacketCapacity = 0; });
    mutated([](auto& plan) { plan.initialAcquiringAudioPacketCapacity = 0; });
    mutated([](auto& plan) { plan.initialAcquiringVideoByteCapacity = 0; });
    mutated([](auto& plan) { plan.initialAcquiringAudioByteCapacity = 0; });
    mutated([](auto& plan) { plan.maximumAcquiringVideoPacketBytes = 0; });
    mutated([](auto& plan) { plan.maximumAcquiringAudioPacketBytes = 0; });
    mutated([](auto& plan) {
        plan.maximumAcquiringVideoPacketBytes =
            plan.initialAcquiringVideoByteCapacity + 1;
    });
    mutated([](auto& plan) {
        plan.maximumAcquiringAudioPacketBytes =
            plan.initialAcquiringAudioByteCapacity + 1;
    });
    mutated([](auto& plan) {
        plan.packetOriginPolicy = static_cast<MediaTsPacketOriginPolicy>(99);
    });
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
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = MediaRunningTime::fromNanoseconds(40'000'000);
    return request;
}

MediaRealtimeRtpTranscodeRequest completeAvSyncRtpRequest()
{
    auto request = avSyncRtpRequest();
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:5004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    request.input.audioRtp.url = "rtp://127.0.0.1:5006";
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.host = "127.0.0.1";
    request.output.basePort = 6000;
    request.output.sdpPath = "planner-av-sync.sdp";
    request.output.packetSize = 1200;
    request.parameters.execution.disableHardware = true;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'000;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.channels = 2;
    request.parameters.queues.metadata = 4;
    request.parameters.queues.packet = 4;
    request.parameters.queues.frame = 4;
    request.parameters.queues.mux = 4;
    return request;
}

void testPlannedProductAndRuntimeRejectSynchronizedAudioPacketCopy(
    TestContext& ctx)
{
    auto outerResult = MediaRealtimeRtpTranscodePlanner::plan(
        completeAvSyncRtpRequest());
    EXPECT_TRUE(ctx, outerResult);
    if (!outerResult || !outerResult.value().avSyncRuntime) return;

    MediaInputAudioStreamInfo source;
    source.streamIndex = 1;
    source.codecName = "aac";
    source.profile = MediaAudioProfile::knownAacLow();
    source.sampleRate = 48'000;
    source.channels = 2;
    source.channelLayout = "stereo";
    source.sampleFormat = "fltp";
    source.bitrateBitsPerSecond = 320'000;
    source.maximumAccessUnitSamples = 1024;
    MediaAudioPipelinePlannerOptions copyOptions(true);
    copyOptions.requestedCodecName = "aac";
    auto copy = MediaAudioPipelinePlanner::planKnownAudio(source, copyOptions);
    EXPECT_TRUE(ctx, copy);
    if (!copy) return;
    EXPECT_EQ(ctx, copy.value().branchMode, MediaBranchMode::CopyPacket);

    auto outer = std::move(outerResult).value();
    outer.audioPlan = std::move(copy).value();
    auto validation =
        MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer);
    EXPECT_FALSE(ctx, validation);
    if (!validation) {
        EXPECT_EQ(ctx, validation.error().code, ::media::ErrorCode::Unsupported);
    }

    auto synchronization = outer.avSyncRuntime->synchronization;
    outer.avSyncRuntime.reset();
    auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
        outer, std::move(synchronization));
    EXPECT_FALSE(ctx, runtime);
    if (!runtime) {
        EXPECT_EQ(ctx, runtime.error().code, ::media::ErrorCode::Unsupported);
    }
}

void testRealtimePlannerProducesCompleteAvSyncRuntimeProduct(TestContext& ctx)
{
    const auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        completeAvSyncRtpRequest());
    EXPECT_TRUE(ctx, planned);
    if (!planned) {
        std::cerr << planned.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, planned.value().avSyncRuntime.has_value());
    EXPECT_EQ(ctx, planned.value().audioPlan.branchMode,
              MediaBranchMode::TranscodeFrame);
    EXPECT_TRUE(ctx, planned.value().audioPlan.selectedDecoder.has_value());
    EXPECT_TRUE(ctx, planned.value().audioPlan.selectedResampler.has_value());
    if (planned.value().audioPlan.selectedDecoder &&
        planned.value().audioPlan.selectedResampler) {
        const auto& decoder = *planned.value().audioPlan.selectedDecoder;
        const auto& resampler = *planned.value().audioPlan.selectedResampler;
        EXPECT_TRUE(ctx, !decoder.name.empty());
        EXPECT_TRUE(ctx, decoder.delayOutputSamples >= 0);
        EXPECT_EQ(ctx, decoder.maximumOutputBlockInputSamples, 1024);
        EXPECT_EQ(ctx, resampler.maximumInputBlockSamples,
                  decoder.maximumOutputBlockInputSamples);
        EXPECT_TRUE(ctx, resampler.maximumOutputBlockSamples >=
            (decoder.maximumOutputBlockInputSamples *
                 resampler.outputSampleRate + resampler.inputSampleRate - 1) /
                resampler.inputSampleRate);
        auto incompleteDecoder = decoder;
        incompleteDecoder.maximumOutputBlockInputSamples = 0;
        EXPECT_FALSE(ctx, MediaAudioResamplerCapabilityProvider::verify(
                              incompleteDecoder,
                              *planned.value().audioPlan.resolvedOutput));
    }
    const std::array<std::uint8_t, 2> aacLc48kStereo{0x11, 0x90};
    EXPECT_FALSE(ctx, MediaAudioDecoderCapabilityProvider::verifyAacAudioSpecificConfig(
                          44'100, 2, aacLc48kStereo));
    if (!planned.value().avSyncRuntime) return;
    const auto& runtime = *planned.value().avSyncRuntime;
    EXPECT_EQ(ctx, runtime.groupKey.value(), std::string("realtime.av"));
    EXPECT_EQ(ctx, runtime.outputAdapter,
              MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp);
    EXPECT_EQ(ctx, runtime.audioCorrection.epochOutputSampleIndex,
              std::int64_t{0});
    EXPECT_EQ(ctx,
              runtime.edgePolicies.audioDriftTransaction.queuePolicy.capacity,
              runtime.queues.frame);
    EXPECT_EQ(ctx,
              runtime.edgePolicies.audioDriftTransaction.queuePolicy
                  .overflowPolicy,
              MediaQueueOverflowPolicy::BlockProducer);
    EXPECT_EQ(ctx,
              runtime.edgePolicies.audioDriftTransaction.queuePolicy
                  .orderingPolicy,
              MediaQueueOrderingPolicy::Fifo);
    EXPECT_TRUE(ctx,
                runtime.edgePolicies.audioDriftTransaction.queuePolicy
                    .preserveOrdering);
    EXPECT_TRUE(ctx,
                runtime.audioCorrection.commandLeadSamples >
                    runtime.audioCorrection.worstCaseInFlightSamples +
                        runtime.audioCorrection.mailboxDeliveryMarginSamples +
                        runtime.audioCorrection.maximumResamplerOutputBlockSamples);
    EXPECT_TRUE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                         planned.value()));
    EXPECT_EQ(ctx, runtime.transition.participants.size(), std::size_t{5});
    EXPECT_EQ(ctx, runtime.transition.participants[0].participant,
              MediaAvGenerationParticipant::CanonicalLineage);
    EXPECT_EQ(ctx, runtime.transition.participants[0].requiredChildren,
              (std::vector<std::string>{
                  "startup_generation_state",
                  "video_decode",
                  "video_frame_rate",
                  "video_filter",
                  "video_encode",
                  "audio_decoder_lineage_registry",
                  "audio_startup_trim_lineage_registry",
                  "audio_resampler_lineage_registry",
                  "audio_encoder_lineage_registry",
                  "audio_encoded_canonicalizer"}));
    EXPECT_EQ(ctx, runtime.transition.participants[1].requiredChildren,
              (std::vector<std::string>{
                  "audio_correction_generation_state"}));
    const auto& rtp = std::get<MediaSeparateRtpOutputRuntimePlan>(
        runtime.protocolOutput);
    EXPECT_EQ(ctx, rtp.video.packetization.codecName(), std::string("h264"));
    EXPECT_EQ(ctx, rtp.video.packetization.streamTimeBaseDenominator(), 90'000);
    EXPECT_EQ(ctx, rtp.video.transport.remoteRtpEndpoint().port(),
              std::uint16_t{6000});
    EXPECT_EQ(ctx, rtp.audio.transport.remoteRtpEndpoint().port(),
              std::uint16_t{6002});
    EXPECT_EQ(ctx, planned.value().videoOutput.url,
              std::string("rtp://127.0.0.1:6000?localrtpport=0&localrtcpport=0"));
}

void testDecoderDelayUsesSelectedOutputSampleDomain(TestContext& ctx)
{
    auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        completeAvSyncRtpRequest());
    EXPECT_TRUE(ctx, planned);
    if (!planned || !planned.value().audioPlan.selectedDecoder ||
        !planned.value().audioPlan.selectedResampler) return;
    auto& outer = planned.value();
    outer.audioPlan.selectedDecoder->inputSampleRate = 96'000;
    outer.audioPlan.selectedDecoder->outputSampleRate = 48'000;
    outer.audioPlan.selectedDecoder->delayOutputSamples = 480;
    outer.audioPlan.selectedResampler->inputSampleRate = 48'000;
    auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::plan(outer);
    EXPECT_TRUE(ctx, bounds);
    if (bounds) EXPECT_EQ(ctx, bounds.value().decoderDelaySamples, 480);
}

void testRealtimeAvSyncRuntimeProductRejectsIndependentMutations(TestContext& ctx)
{
    auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        completeAvSyncRtpRequest());
    EXPECT_TRUE(ctx, planned);
    if (!planned || !planned.value().avSyncRuntime) return;
    auto& outer = planned.value();
    auto requiredRuntime = std::move(outer.avSyncRuntime);
    outer.avSyncRuntime.reset();
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                          outer));
    outer.avSyncRuntime = std::move(requiredRuntime);
    auto& runtime = *outer.avSyncRuntime;
    const auto expectInvalid = [&ctx, &outer]() {
        EXPECT_FALSE(ctx,
                     MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                         outer));
    };

    const auto selectedComponentBounds = outer.avSyncComponentBounds;
    outer.avSyncComponentBounds.reset();
    expectInvalid();
    outer.avSyncComponentBounds = selectedComponentBounds;
    ++outer.avSyncComponentBounds->decodeQueueSamples;
    expectInvalid();
    outer.avSyncComponentBounds = selectedComponentBounds;
    const auto selectedDecoderDelay =
        outer.audioPlan.selectedDecoder->delayOutputSamples;
    ++outer.audioPlan.selectedDecoder->delayOutputSamples;
    expectInvalid();
    outer.audioPlan.selectedDecoder->delayOutputSamples = selectedDecoderDelay;
    const auto selectedResamplerBlock =
        outer.audioPlan.selectedResampler->maximumOutputBlockSamples;
    ++outer.audioPlan.selectedResampler->maximumOutputBlockSamples;
    expectInvalid();
    outer.audioPlan.selectedResampler->maximumOutputBlockSamples =
        selectedResamplerBlock;

    outer.videoPacketCopyNormalizationRequired = true;
    expectInvalid();
    outer.videoPacketCopyNormalizationRequired = false;
    outer.audioPacketNormalizationRequired = true;
    expectInvalid();
    outer.audioPacketNormalizationRequired = false;

    outer.input.rtpTransport->requireCname = true;
    expectInvalid();
    outer.input.rtpTransport->requireCname = false;
    outer.audioInput.rtpTransport->rtcpCompositionMode =
        MediaRtcpCompositionMode::StrictCompoundRfc3550;
    expectInvalid();
    outer.audioInput.rtpTransport->rtcpCompositionMode =
        MediaRtcpCompositionMode::ReducedSizeRfc5506;
    ++outer.input.rtpTransport->clockRate;
    expectInvalid();
    --outer.input.rtpTransport->clockRate;
    ++outer.audioInput.rtpTransport->payloadType;
    expectInvalid();
    --outer.audioInput.rtpTransport->payloadType;

    const auto selectedPlanningFacts = runtime.planningFacts;
    runtime.planningFacts.outputSampleRate.reset();
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.decoderDelaySamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.encoderLookaheadSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.decodeQueueSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.resampleQueueSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.encodeQueueSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.schedulerQueueSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.protocolBatchSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.mailboxDeliveryMarginSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.maximumResamplerOutputBlockSamples;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    ++*runtime.planningFacts.mailboxCapacity;
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    runtime.planningFacts.acknowledgementTimeout =
        MediaRunningTime::fromNanoseconds(
            runtime.planningFacts.acknowledgementTimeout->nanoseconds() + 1);
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;
    runtime.planningFacts.terminalDrainWindow =
        MediaRunningTime::fromNanoseconds(
            runtime.planningFacts.terminalDrainWindow->nanoseconds() + 1);
    expectInvalid();
    runtime.planningFacts = selectedPlanningFacts;

    auto alternateFacts = selectedPlanningFacts;
    ++*alternateFacts.decodeQueueSamples;
    auto alternateCorrection = MediaAudioCorrectionReachabilityPlanner::plan(
        runtime.synchronization, alternateFacts);
    EXPECT_TRUE(ctx, alternateCorrection);
    if (alternateCorrection) {
        const auto selectedCorrection = runtime.audioCorrection;
        const auto selectedCommandLead =
            runtime.synchronization.audioServo.commandLeadNs;
        const auto selectedCompensation =
            runtime.synchronization.audioServo.compensationWindowNs;
        const auto selectedFrequency =
            runtime.synchronization.audioServo.frequencyFilterTimeConstantNs;
        runtime.planningFacts = alternateFacts;
        runtime.audioCorrection = alternateCorrection.value().correction;
        runtime.synchronization.audioServo.commandLeadNs =
            alternateCorrection.value().commandLead;
        runtime.synchronization.audioServo.compensationWindowNs =
            alternateCorrection.value().compensationWindow;
        runtime.synchronization.audioServo.frequencyFilterTimeConstantNs =
            alternateCorrection.value().frequencyFilterTimeConstant;
        expectInvalid();
        runtime.planningFacts = selectedPlanningFacts;
        runtime.audioCorrection = selectedCorrection;
        runtime.synchronization.audioServo.commandLeadNs = selectedCommandLead;
        runtime.synchronization.audioServo.compensationWindowNs =
            selectedCompensation;
        runtime.synchronization.audioServo.frequencyFilterTimeConstantNs =
            selectedFrequency;
    }

    runtime.groupKey = MediaAvSyncGroupKey("wrong");
    expectInvalid();
    runtime.groupKey = MediaAvSyncGroupKey("realtime.av");
    ++runtime.queues.metadata;
    expectInvalid();
    --runtime.queues.metadata;
    ++runtime.queues.packet;
    expectInvalid();
    --runtime.queues.packet;
    ++runtime.queues.frame;
    expectInvalid();
    --runtime.queues.frame;
    ++runtime.queues.mux;
    expectInvalid();
    --runtime.queues.mux;
    ++runtime.edgePolicies.metadata.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.metadata.queuePolicy.capacity;
    ++runtime.edgePolicies.packet.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.packet.queuePolicy.capacity;
    ++runtime.edgePolicies.videoPacket.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.videoPacket.queuePolicy.capacity;
    ++runtime.edgePolicies.audioPacket.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.audioPacket.queuePolicy.capacity;
    ++runtime.edgePolicies.synchronizedPacket.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.synchronizedPacket.queuePolicy.capacity;
    ++runtime.edgePolicies.audioDriftTransaction.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.audioDriftTransaction.queuePolicy.capacity;
    runtime.edgePolicies.synchronizedPacket.queuePolicy.overflowPolicy =
        MediaQueueOverflowPolicy::DropOldest;
    expectInvalid();
    runtime.edgePolicies.synchronizedPacket.queuePolicy.overflowPolicy =
        MediaQueueOverflowPolicy::BlockProducer;
    const MediaEdgePolicy synchronizedPacketPolicy =
        runtime.edgePolicies.synchronizedPacket;
    const auto expectSynchronizedPacketMutationInvalid =
        [&runtime, &expectInvalid, &synchronizedPacketPolicy](auto mutate) {
            mutate(runtime.edgePolicies.synchronizedPacket);
            expectInvalid();
            runtime.edgePolicies.synchronizedPacket = synchronizedPacketPolicy;
        };
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.queuePolicy.preserveOrdering = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.queuePolicy.allowFlushControlBypass = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        ++policy.queuePolicy.reserveCapacity;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        ++policy.queuePolicy.backpressurePolicy.lowWatermark;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        ++policy.queuePolicy.backpressurePolicy.highWatermark;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.queuePolicy.backpressurePolicy.realtimePriority = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.queuePolicy.backpressurePolicy.propagateUpstream = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.queuePolicy.backpressurePolicy.reportMetrics = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.backpressurePolicy.propagateUpstream = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.backpressurePolicy.reportMetrics = false;
    });
    expectSynchronizedPacketMutationInvalid([](MediaEdgePolicy& policy) {
        policy.bufferPolicy.ownership = MediaBufferOwnership::MoveOnly;
    });
    ++runtime.edgePolicies.videoFrame.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.videoFrame.queuePolicy.capacity;
    ++runtime.edgePolicies.audioFrame.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.audioFrame.queuePolicy.capacity;
    ++runtime.edgePolicies.mux.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.mux.queuePolicy.capacity;
    ++runtime.edgePolicies.videoMux.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.videoMux.queuePolicy.capacity;
    ++runtime.edgePolicies.audioMux.queuePolicy.capacity;
    expectInvalid();
    --runtime.edgePolicies.audioMux.queuePolicy.capacity;
    runtime.threadingPolicy.mode = MediaThreadingMode::SingleThreaded;
    expectInvalid();
    runtime.threadingPolicy.mode = MediaThreadingMode::PerNodeWorker;
    runtime.threadingPolicy.priority = MediaThreadPriority::Normal;
    expectInvalid();
    runtime.threadingPolicy.priority = MediaThreadPriority::High;
    runtime.threadingPolicy.maxWorkerThreads = 1;
    expectInvalid();
    runtime.threadingPolicy.maxWorkerThreads = 0;
    runtime.threadingPolicy.pinWorkers = true;
    expectInvalid();
    runtime.threadingPolicy.pinWorkers = false;
    runtime.threadingPolicy.collectWorkerMetrics = false;
    expectInvalid();
    runtime.threadingPolicy.collectWorkerMetrics = true;
    runtime.transition.participants[0].requiredChildren[0] = "wrong";
    expectInvalid();
    runtime.transition.participants[0].requiredChildren[0] =
        "startup_generation_state";
    ++runtime.audioCorrection.mailboxCapacity;
    expectInvalid();
    --runtime.audioCorrection.mailboxCapacity;
    const auto maximumBlock =
        runtime.audioCorrection.maximumResamplerOutputBlockSamples;
    runtime.audioCorrection.maximumResamplerOutputBlockSamples =
        runtime.audioCorrection.commandLeadSamples;
    expectInvalid();
    runtime.audioCorrection.maximumResamplerOutputBlockSamples = maximumBlock;
    const auto worstCase = runtime.audioCorrection.worstCaseInFlightSamples;
    runtime.audioCorrection.worstCaseInFlightSamples = -1;
    expectInvalid();
    runtime.audioCorrection.worstCaseInFlightSamples = worstCase;
    const auto mailboxMargin =
        runtime.audioCorrection.mailboxDeliveryMarginSamples;
    runtime.audioCorrection.mailboxDeliveryMarginSamples = 0;
    expectInvalid();
    runtime.audioCorrection.mailboxDeliveryMarginSamples = mailboxMargin;
    const auto commandLead = runtime.audioCorrection.commandLeadSamples;
    runtime.audioCorrection.commandLeadSamples = worstCase + mailboxMargin +
        maximumBlock;
    expectInvalid();
    runtime.audioCorrection.commandLeadSamples = commandLead;
    const auto commandLeadTime =
        runtime.synchronization.audioServo.commandLeadNs;
    runtime.synchronization.audioServo.commandLeadNs =
        MediaRunningTime::fromNanoseconds(
            commandLeadTime->nanoseconds() + 1);
    expectInvalid();
    runtime.synchronization.audioServo.commandLeadNs = commandLeadTime;
    const auto compensationTime =
        runtime.synchronization.audioServo.compensationWindowNs;
    runtime.synchronization.audioServo.compensationWindowNs =
        MediaRunningTime::fromNanoseconds(
            compensationTime->nanoseconds() + 1);
    expectInvalid();
    runtime.synchronization.audioServo.compensationWindowNs = compensationTime;
    const auto frequencyTime =
        runtime.synchronization.audioServo.frequencyFilterTimeConstantNs;
    runtime.synchronization.audioServo.frequencyFilterTimeConstantNs =
        MediaRunningTime::fromNanoseconds(
            frequencyTime->nanoseconds() + 1);
    expectInvalid();
    runtime.synchronization.audioServo.frequencyFilterTimeConstantNs =
        frequencyTime;
    runtime.audioCorrection.epochOutputSampleIndex = 1;
    expectInvalid();
    runtime.audioCorrection.epochOutputSampleIndex = 0;
    const auto outputSampleRate = runtime.audioCorrection.outputSampleRate;
    runtime.audioCorrection.outputSampleRate = 0;
    expectInvalid();
    runtime.audioCorrection.outputSampleRate = outputSampleRate;
    const auto protocolBatch = runtime.audioCorrection.protocolBatchSamples;
    ++runtime.audioCorrection.protocolBatchSamples;
    expectInvalid();
    runtime.audioCorrection.protocolBatchSamples = protocolBatch;
    auto removedParticipant = std::move(runtime.transition.participants.back());
    runtime.transition.participants.pop_back();
    expectInvalid();
    runtime.transition.participants.push_back(std::move(removedParticipant));
    runtime.transition.participants[4].participant =
        MediaAvGenerationParticipant::ProjectMpegTsOutput;
    expectInvalid();
    runtime.transition.participants[4].participant =
        MediaAvGenerationParticipant::RtpAudioOutput;
    const auto acknowledgementTimeout =
        runtime.transition.acknowledgementTimeout;
    runtime.transition.acknowledgementTimeout =
        MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    runtime.transition.acknowledgementTimeout = acknowledgementTimeout;
    const auto terminalDrainWindow = runtime.transition.terminalDrainWindow;
    runtime.transition.terminalDrainWindow =
        MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    runtime.transition.terminalDrainWindow = terminalDrainWindow;
    const auto topology = runtime.synchronization.topology;
    runtime.synchronization.topology = MediaAvSyncTopology::MpegTsToMpegTs;
    expectInvalid();
    runtime.synchronization.topology = topology;
    outer.outputLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    expectInvalid();
    outer.outputLayout = RealtimeOutputStreamLayout::SeparateStreams;
    runtime.outputAdapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
    expectInvalid();
    runtime.outputAdapter =
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp;
    auto& rtp = std::get<MediaSeparateRtpOutputRuntimePlan>(
        runtime.protocolOutput);
    rtp.sdp.path.clear();
    expectInvalid();
    rtp.sdp.path = outer.sdp.path;
    const auto originalVideoPacketization = rtp.video.packetization;
    rtp.video.stream = MediaScheduledStream::Audio;
    expectInvalid();
    rtp.video.stream = MediaScheduledStream::Video;
    auto changedVideoTimeBaseNumerator = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, "h264", 2, 90'000,
        originalVideoPacketization.payloadType(),
        originalVideoPacketization.maximumDatagramBytes());
    EXPECT_TRUE(ctx, changedVideoTimeBaseNumerator);
    rtp.video.packetization = std::move(changedVideoTimeBaseNumerator).value();
    expectInvalid();
    rtp.video.packetization = originalVideoPacketization;
    auto changedVideoTimeBase = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, "h264", 1, 90'001,
        originalVideoPacketization.payloadType(),
        originalVideoPacketization.maximumDatagramBytes());
    EXPECT_TRUE(ctx, changedVideoTimeBase);
    rtp.video.packetization = std::move(changedVideoTimeBase).value();
    expectInvalid();
    rtp.video.packetization = originalVideoPacketization;
    auto changedVideoPayload = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, "h264", 1, 90'000,
        originalVideoPacketization.payloadType() + 1,
        originalVideoPacketization.maximumDatagramBytes());
    EXPECT_TRUE(ctx, changedVideoPayload);
    rtp.video.packetization = std::move(changedVideoPayload).value();
    expectInvalid();
    rtp.video.packetization = originalVideoPacketization;
    auto changedVideoDatagram = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, "h264", 1, 90'000,
        originalVideoPacketization.payloadType(),
        originalVideoPacketization.maximumDatagramBytes() + 1);
    EXPECT_TRUE(ctx, changedVideoDatagram);
    rtp.video.packetization = std::move(changedVideoDatagram).value();
    expectInvalid();
    rtp.video.packetization = originalVideoPacketization;
    EXPECT_FALSE(ctx, MediaScheduledRtpPacketizationPlan::create(
                          MediaStreamKind::Video, "vp9", 1, 90'000,
                          originalVideoPacketization.payloadType(),
                          originalVideoPacketization.maximumDatagramBytes()));
    ++rtp.video.ssrc;
    expectInvalid();
    --rtp.video.ssrc;
    ++rtp.video.baseTimestamp;
    expectInvalid();
    --rtp.video.baseTimestamp;
    ++rtp.video.clockRate;
    expectInvalid();
    --rtp.video.clockRate;
    rtp.video.cname.clear();
    expectInvalid();
    rtp.video.cname = *runtime.synchronization.rtp->videoOutput.cname;
    const auto videoLead = rtp.video.senderLead;
    rtp.video.senderLead = MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    rtp.video.senderLead = videoLead;
    const auto videoReportInterval = rtp.video.senderReportInterval;
    rtp.video.senderReportInterval = MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    rtp.video.senderReportInterval = videoReportInterval;
    const auto reportInterval = rtp.audio.senderReportInterval;
    rtp.audio.senderReportInterval = MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    rtp.audio.senderReportInterval = reportInterval;
    const auto originalAudioPacketization = rtp.audio.packetization;
    rtp.audio.stream = MediaScheduledStream::Video;
    expectInvalid();
    rtp.audio.stream = MediaScheduledStream::Audio;
    auto changedAudioTimeBaseNumerator = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 2, 48'000,
        originalAudioPacketization.payloadType(),
        originalAudioPacketization.maximumDatagramBytes(),
        originalAudioPacketization.maximumAccessUnitSamples());
    EXPECT_TRUE(ctx, changedAudioTimeBaseNumerator);
    rtp.audio.packetization = std::move(changedAudioTimeBaseNumerator).value();
    expectInvalid();
    rtp.audio.packetization = originalAudioPacketization;
    auto changedAudioTimeBaseDenominator = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 1, 48'001,
        originalAudioPacketization.payloadType(),
        originalAudioPacketization.maximumDatagramBytes(),
        originalAudioPacketization.maximumAccessUnitSamples());
    EXPECT_TRUE(ctx, changedAudioTimeBaseDenominator);
    rtp.audio.packetization = std::move(changedAudioTimeBaseDenominator).value();
    expectInvalid();
    rtp.audio.packetization = originalAudioPacketization;
    auto changedAudioPayload = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 1, 48'000,
        originalAudioPacketization.payloadType() + 1,
        originalAudioPacketization.maximumDatagramBytes(),
        originalAudioPacketization.maximumAccessUnitSamples());
    EXPECT_TRUE(ctx, changedAudioPayload);
    rtp.audio.packetization = std::move(changedAudioPayload).value();
    expectInvalid();
    rtp.audio.packetization = originalAudioPacketization;
    auto changedAudioAccessUnit = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 1, 48'000,
        originalAudioPacketization.payloadType(),
        originalAudioPacketization.maximumDatagramBytes(),
        *originalAudioPacketization.maximumAccessUnitSamples() + 1);
    EXPECT_TRUE(ctx, changedAudioAccessUnit);
    rtp.audio.packetization = std::move(changedAudioAccessUnit).value();
    expectInvalid();
    rtp.audio.packetization = originalAudioPacketization;
    auto changedAudioDatagram = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 1, 48'000,
        originalAudioPacketization.payloadType(),
        originalAudioPacketization.maximumDatagramBytes() + 1,
        originalAudioPacketization.maximumAccessUnitSamples());
    EXPECT_TRUE(ctx, changedAudioDatagram);
    rtp.audio.packetization = std::move(changedAudioDatagram).value();
    expectInvalid();
    rtp.audio.packetization = originalAudioPacketization;
    EXPECT_FALSE(ctx, MediaScheduledRtpPacketizationPlan::create(
                          MediaStreamKind::Audio, "opus", 1, 48'000,
                          originalAudioPacketization.payloadType(),
                          originalAudioPacketization.maximumDatagramBytes(),
                          originalAudioPacketization.maximumAccessUnitSamples()));
    EXPECT_FALSE(ctx, MediaScheduledRtpPacketizationPlan::create(
                          MediaStreamKind::Audio, "aac", 1, 48'000,
                          originalAudioPacketization.payloadType(),
                          originalAudioPacketization.maximumDatagramBytes()));
    ++rtp.audio.ssrc;
    expectInvalid();
    --rtp.audio.ssrc;
    ++rtp.audio.baseTimestamp;
    expectInvalid();
    --rtp.audio.baseTimestamp;
    ++rtp.audio.clockRate;
    expectInvalid();
    --rtp.audio.clockRate;
    const auto audioCname = rtp.audio.cname;
    rtp.audio.cname.clear();
    expectInvalid();
    rtp.audio.cname = audioCname;
    const auto audioLead = rtp.audio.senderLead;
    rtp.audio.senderLead = MediaRunningTime::fromNanoseconds(0);
    expectInvalid();
    rtp.audio.senderLead = audioLead;

    const auto videoFamily = rtp.video.transport.addressFamily();
    const auto videoLocal = rtp.video.transport.localNumericAddress();
    const auto videoRemote =
        rtp.video.transport.remoteRtpEndpoint().numericAddress();
    const auto videoRtpPort = rtp.video.transport.remoteRtpEndpoint().port();
    const auto videoRtcpPort = rtp.video.transport.remoteRtcpEndpoint().port();
    const auto videoSendBuffer = rtp.video.transport.sendBufferBytes();
    const auto videoMaximumDatagram =
        rtp.video.transport.maximumDatagramBytes();
    const auto installVideoTransport = [&](MediaIpAddressFamily family,
                                           std::string local,
                                           std::string remote,
                                           std::uint16_t rtpPort,
                                           std::uint16_t rtcpPort,
                                           MediaRtpUdpLocalPortPolicy localPorts,
                                           int sendBuffer,
                                           std::size_t maximumDatagram) {
        auto transport = MediaRtpUdpSenderConfig::create(
            family, std::move(local), std::move(remote), rtpPort, rtcpPort,
            std::move(localPorts), sendBuffer, maximumDatagram,
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
        EXPECT_TRUE(ctx, transport);
        if (transport) rtp.video.transport = std::move(transport).value();
    };
    const auto restoreVideoTransport = [&]() {
        installVideoTransport(
            videoFamily, videoLocal, videoRemote, videoRtpPort, videoRtcpPort,
            MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
            videoSendBuffer, videoMaximumDatagram);
    };
    installVideoTransport(
        MediaIpAddressFamily::Ipv6, "::", "::1", videoRtpPort,
        videoRtcpPort, MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        videoSendBuffer, videoMaximumDatagram);
    expectInvalid();
    restoreVideoTransport();
    installVideoTransport(
        videoFamily, "127.0.0.1", videoRemote, videoRtpPort, videoRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), videoSendBuffer,
        videoMaximumDatagram);
    expectInvalid();
    restoreVideoTransport();
    installVideoTransport(
        videoFamily, videoLocal, "127.0.0.2", videoRtpPort, videoRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), videoSendBuffer,
        videoMaximumDatagram);
    expectInvalid();
    restoreVideoTransport();
    installVideoTransport(
        videoFamily, videoLocal, videoRemote,
        static_cast<std::uint16_t>(videoRtpPort + 10),
        static_cast<std::uint16_t>(videoRtcpPort + 10),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), videoSendBuffer,
        videoMaximumDatagram);
    expectInvalid();
    restoreVideoTransport();
    auto fixedPorts = MediaRtpUdpLocalPortPolicy::fixedAdjacent(7000, 7001);
    EXPECT_TRUE(ctx, fixedPorts);
    if (fixedPorts) {
        installVideoTransport(
            videoFamily, videoLocal, videoRemote, videoRtpPort, videoRtcpPort,
            std::move(fixedPorts).value(), videoSendBuffer,
            videoMaximumDatagram);
        expectInvalid();
        restoreVideoTransport();
    }
    installVideoTransport(
        videoFamily, videoLocal, videoRemote, videoRtpPort, videoRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        videoSendBuffer + 1, videoMaximumDatagram);
    expectInvalid();
    restoreVideoTransport();
    installVideoTransport(
        videoFamily, videoLocal, videoRemote, videoRtpPort, videoRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), videoSendBuffer,
        videoMaximumDatagram - 1);
    expectInvalid();
    restoreVideoTransport();
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
                          MediaIpAddressFamily::Ipv4, "::", videoRemote,
                          videoRtpPort, videoRtcpPort,
                          MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
                          videoSendBuffer, videoMaximumDatagram,
                          MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure));
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
                          videoFamily, videoLocal, videoRemote,
                          videoRtpPort, videoRtcpPort,
                          MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
                          videoSendBuffer, videoMaximumDatagram,
                          static_cast<MediaUdpSenderIoBehavior>(0xFF)));

    const auto audioFamily = rtp.audio.transport.addressFamily();
    const auto audioLocal = rtp.audio.transport.localNumericAddress();
    const auto audioRemote =
        rtp.audio.transport.remoteRtpEndpoint().numericAddress();
    const auto audioRtpPort = rtp.audio.transport.remoteRtpEndpoint().port();
    const auto audioRtcpPort = rtp.audio.transport.remoteRtcpEndpoint().port();
    const auto audioSendBuffer = rtp.audio.transport.sendBufferBytes();
    const auto audioMaximumDatagram =
        rtp.audio.transport.maximumDatagramBytes();
    const auto installAudioTransport = [&](MediaIpAddressFamily family,
                                           std::string local,
                                           std::string remote,
                                           std::uint16_t rtpPort,
                                           std::uint16_t rtcpPort,
                                           MediaRtpUdpLocalPortPolicy localPorts,
                                           int sendBuffer,
                                           std::size_t maximumDatagram) {
        auto transport = MediaRtpUdpSenderConfig::create(
            family, std::move(local), std::move(remote), rtpPort, rtcpPort,
            std::move(localPorts), sendBuffer, maximumDatagram,
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
        EXPECT_TRUE(ctx, transport);
        if (transport) rtp.audio.transport = std::move(transport).value();
    };
    const auto restoreAudioTransport = [&]() {
        installAudioTransport(
            audioFamily, audioLocal, audioRemote, audioRtpPort, audioRtcpPort,
            MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
            audioSendBuffer, audioMaximumDatagram);
    };
    installAudioTransport(
        MediaIpAddressFamily::Ipv6, "::", "::1", audioRtpPort,
        audioRtcpPort, MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        audioSendBuffer, audioMaximumDatagram);
    expectInvalid();
    restoreAudioTransport();
    installAudioTransport(
        audioFamily, "127.0.0.1", audioRemote, audioRtpPort, audioRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), audioSendBuffer,
        audioMaximumDatagram);
    expectInvalid();
    restoreAudioTransport();
    installAudioTransport(
        audioFamily, audioLocal, "127.0.0.2", audioRtpPort, audioRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), audioSendBuffer,
        audioMaximumDatagram);
    expectInvalid();
    restoreAudioTransport();
    installAudioTransport(
        audioFamily, audioLocal, audioRemote,
        static_cast<std::uint16_t>(audioRtpPort + 2),
        static_cast<std::uint16_t>(audioRtcpPort + 2),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), audioSendBuffer,
        audioMaximumDatagram);
    expectInvalid();
    restoreAudioTransport();
    auto audioFixedPorts = MediaRtpUdpLocalPortPolicy::fixedAdjacent(7002, 7003);
    EXPECT_TRUE(ctx, audioFixedPorts);
    if (audioFixedPorts) {
        installAudioTransport(
            audioFamily, audioLocal, audioRemote, audioRtpPort, audioRtcpPort,
            std::move(audioFixedPorts).value(), audioSendBuffer,
            audioMaximumDatagram);
        expectInvalid();
        restoreAudioTransport();
    }
    installAudioTransport(
        audioFamily, audioLocal, audioRemote, audioRtpPort, audioRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        audioSendBuffer + 1, audioMaximumDatagram);
    expectInvalid();
    restoreAudioTransport();
    installAudioTransport(
        audioFamily, audioLocal, audioRemote, audioRtpPort, audioRtcpPort,
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(), audioSendBuffer,
        audioMaximumDatagram - 1);
    expectInvalid();
    restoreAudioTransport();
    outer.videoOutput.writePacingEnabled = true;
    expectInvalid();
    outer.videoOutput.writePacingEnabled = false;
    outer.videoOutput.writePacingBytesPerSecond = 1;
    expectInvalid();
    outer.videoOutput.writePacingBytesPerSecond = 0;
    outer.videoOutput.writePacingBurstBytes = 1;
    expectInvalid();
    outer.videoOutput.writePacingBurstBytes = 0;
    outer.audioOutput.writePacingEnabled = true;
    expectInvalid();
    outer.audioOutput.writePacingEnabled = false;
    outer.videoMux.pacingPolicy.enablePacing = true;
    expectInvalid();
    outer.videoMux.pacingPolicy.enablePacing = false;
    outer.audioMux.startupDelayMs = 1;
    expectInvalid();
    outer.audioMux.startupDelayMs = 0;
    outer.avStartBarrier.expectVideo = true;
    expectInvalid();
    outer.avStartBarrier.expectVideo = false;

    const auto expectMissingFacts = [&ctx, &outer, &runtime]() {
        EXPECT_FALSE(ctx, MediaRealtimeAvSyncPlanningFactsResolver::resolve(
                              outer, runtime.synchronization));
    };
    auto missingRuntimeOutputFormat = std::move(outer.audioPlan.resolvedOutput);
    outer.audioPlan.resolvedOutput.reset();
    expectInvalid();
    outer.audioPlan.resolvedOutput = std::move(missingRuntimeOutputFormat);
    const auto audioPacketization = outer.audioOutput.scheduledPacketization;
    outer.audioOutput.scheduledPacketization.reset();
    expectMissingFacts();
    outer.audioOutput.scheduledPacketization = audioPacketization;
    const auto componentBounds = outer.avSyncComponentBounds;
    outer.avSyncComponentBounds.reset();
    expectMissingFacts();
    outer.avSyncComponentBounds = componentBounds;
    const auto decodeQueueSamples = outer.avSyncComponentBounds->decodeQueueSamples;
    outer.avSyncComponentBounds->decodeQueueSamples = 0;
    expectMissingFacts();
    outer.avSyncComponentBounds->decodeQueueSamples = decodeQueueSamples;
    const auto maximumResamplerBlock =
        outer.avSyncComponentBounds->maximumResamplerOutputBlockSamples;
    outer.avSyncComponentBounds->maximumResamplerOutputBlockSamples = 0;
    expectMissingFacts();
    outer.avSyncComponentBounds->maximumResamplerOutputBlockSamples = maximumResamplerBlock;
    const auto originalBounds = *outer.avSyncComponentBounds;
    outer.avSyncComponentBounds = MediaRealtimeAvSyncComponentBounds{
        17, 101, 103, 107, 109, 127, 131, 137};
    const auto independentFacts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, runtime.synchronization);
    EXPECT_TRUE(ctx, independentFacts);
    if (independentFacts) {
        EXPECT_EQ(ctx, *independentFacts.value().decoderDelaySamples, 17);
        EXPECT_EQ(ctx, *independentFacts.value().decodeQueueSamples, 101);
        EXPECT_EQ(ctx, *independentFacts.value().resampleQueueSamples, 103);
        EXPECT_EQ(ctx, *independentFacts.value().encodeQueueSamples, 107);
        EXPECT_EQ(ctx, *independentFacts.value().schedulerQueueSamples, 109);
        EXPECT_EQ(ctx, *independentFacts.value().protocolBatchSamples, 1024);
        EXPECT_EQ(ctx, *independentFacts.value().mailboxDeliveryMarginSamples, 127);
        EXPECT_EQ(ctx, *independentFacts.value().maximumResamplerOutputBlockSamples, 131);
        EXPECT_EQ(ctx, *independentFacts.value().mailboxCapacity, std::size_t{137});
    }
    outer.avSyncComponentBounds = originalBounds;
    auto resolvedOutput = std::move(outer.audioPlan.resolvedOutput);
    outer.audioPlan.resolvedOutput.reset();
    expectMissingFacts();
    outer.audioPlan.resolvedOutput = std::move(resolvedOutput);
    const auto servoSampleRate =
        runtime.synchronization.audioServo.outputSampleRate;
    runtime.synchronization.audioServo.outputSampleRate.reset();
    expectMissingFacts();
    runtime.synchronization.audioServo.outputSampleRate = servoSampleRate;
    const auto servoCommandLead =
        runtime.synchronization.audioServo.commandLeadNs;
    runtime.synchronization.audioServo.commandLeadNs.reset();
    EXPECT_TRUE(ctx, MediaRealtimeAvSyncPlanningFactsResolver::resolve(
                         outer, runtime.synchronization));
    runtime.synchronization.audioServo.commandLeadNs = servoCommandLead;

    outer.videoOutput.url =
        "rtp://203.0.113.10:9?untrusted_decision=1";
    EXPECT_TRUE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                         outer));

    auto nonNumericHost = completeAvSyncRtpRequest();
    nonNumericHost.output.host = "localhost";
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::plan(nonNumericHost));
}

MediaRealtimeRtpTranscodeRequest avSyncTsRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "planner-av-sync-ts";
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = MediaRunningTime::fromNanoseconds(40'000'000);
    return request;
}

MediaResolvedAudioOutputPlan resolvedAacOutput(MediaAudioProfile profile)
{
    MediaResolvedAudioSource source{
        "aac", profile, 48'000, 2, "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest request;
    auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    auto resolved = MediaResolvedAudioOutputPlan::create(
        target.value(), std::nullopt, 1024);
    return std::move(resolved).value();
}

void testSelectedResamplerPublishesSteadyStateBound(TestContext& ctx)
{
    MediaResolvedAudioSource source{
        "aac", MediaAudioProfile::knownAacLow(), 48'000, 2,
        "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest request;
    request.sampleRate = 44'100;
    auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    EXPECT_TRUE(ctx, target);
    if (!target) return;
    auto output = MediaResolvedAudioOutputPlan::create(
        target.value(),
        MediaSelectedAudioEncoder{
            "aac", "fltp", {44'100}, {AV_PROFILE_AAC_LOW}, 1024, 0},
        std::nullopt);
    EXPECT_TRUE(ctx, output);
    if (!output) return;
    const std::array<std::uint8_t, 2> aacLc48kStereo{0x11, 0x90};
    auto decoder = MediaAudioDecoderCapabilityProvider::verifyAacAudioSpecificConfig(
        48'000, 2, aacLc48kStereo);
    EXPECT_TRUE(ctx, decoder);
    if (!decoder) return;
    auto resampler = MediaAudioResamplerCapabilityProvider::verify(
        decoder.value(), output.value());
    EXPECT_TRUE(ctx, resampler);
    if (!resampler) return;
    const auto freshRateScaled =
        (decoder.value().maximumOutputBlockInputSamples * 44'100 + 47'999) /
        48'000;
    EXPECT_TRUE(ctx, resampler.value().maximumOutputBlockSamples >
                         freshRateScaled);
}

void testAacAdtsDecoderCapabilityDoesNotRequireContainerExtradata(
    TestContext& ctx)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    EXPECT_TRUE(ctx, parameters != nullptr);
    if (!parameters) return;
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = AV_CODEC_ID_AAC;
    parameters->profile = AV_PROFILE_AAC_LOW;
    parameters->sample_rate = 48'000;
    parameters->format = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_default(&parameters->ch_layout, 2);
    EXPECT_TRUE(ctx, parameters->extradata == nullptr);
    EXPECT_EQ(ctx, parameters->extradata_size, 0);

    const auto decoder =
        MediaAudioDecoderCapabilityProvider::verifyAacAdts(*parameters);
    if (!decoder) std::cerr << decoder.error().describe() << '\n';
    EXPECT_TRUE(ctx, decoder);
    if (!decoder) return;
    EXPECT_EQ(ctx, decoder.value().inputSampleRate, 48'000);
    EXPECT_EQ(ctx, decoder.value().outputSampleRate, 48'000);
    EXPECT_EQ(ctx, decoder.value().outputChannels, 2);
    EXPECT_TRUE(ctx, decoder.value().delayOutputSamples >= 0);
    EXPECT_EQ(ctx, decoder.value().maximumOutputBlockInputSamples, 1024);
}

void testOpusRtpDecoderCapabilityUsesPlannedProtocolBound(TestContext& ctx)
{
    constexpr std::int64_t MaximumOpusRtpAccessUnitSamples = 5'760;
    const auto decoder = MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
        48'000, 2, MaximumOpusRtpAccessUnitSamples);
    EXPECT_TRUE(ctx, decoder);
    if (!decoder) return;
    EXPECT_EQ(ctx, decoder.value().inputSampleRate, 48'000);
    EXPECT_EQ(ctx, decoder.value().outputSampleRate, 48'000);
    EXPECT_EQ(ctx, decoder.value().outputChannels, 2);
    EXPECT_TRUE(ctx, decoder.value().delayOutputSamples >= 0);
    EXPECT_EQ(ctx, decoder.value().maximumOutputBlockInputSamples,
              MaximumOpusRtpAccessUnitSamples);
    EXPECT_FALSE(ctx, MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
                          48'000, 2, 0));
    EXPECT_FALSE(ctx, MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
                          44'100, 2, MaximumOpusRtpAccessUnitSamples));
    EXPECT_FALSE(ctx, MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
                          48'000, 3, MaximumOpusRtpAccessUnitSamples));
}

MediaProjectMpegTsResolvedPipelineFacts validTsResolvedFacts()
{
    auto audio = resolvedAacOutput(MediaAudioProfile::knownAacLow());
    return MediaProjectMpegTsResolvedPipelineFacts{
        "h264", MediaEncodedPacketLayout::lengthPrefixed(4).value(),
        std::move(audio)};
}

void testRealtimePlannerProducesCompleteTsAvSyncRuntimeProduct(TestContext& ctx)
{
    auto outerResult = MediaRealtimeRtpTranscodePlanner::plan(
        completeAvSyncRtpRequest());
    EXPECT_TRUE(ctx, outerResult);
    if (!outerResult) return;
    auto outer = std::move(outerResult).value();
    outer.inputType = RealtimeInputType::MpegTsUdp;
    outer.inputLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    outer.outputLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    outer.muxedOutput.url = "udp://127.0.0.1:7000";

    constexpr std::uint64_t MaximumPacketPositionRegressionBytes = 188;
    auto evidenceCapacity = MediaRealtimeTsInputPlan::minimumEvidenceCapacity(
        188, static_cast<std::uint64_t>(outer.input.probeSizeBytes),
        MaximumPacketPositionRegressionBytes);
    EXPECT_TRUE(ctx, evidenceCapacity);
    if (!evidenceCapacity) return;
    auto tsInput = MediaRealtimeTsInputPlan::create(
        188, static_cast<std::uint64_t>(outer.input.probeSizeBytes),
        MaximumPacketPositionRegressionBytes, evidenceCapacity.value(), 2);
    EXPECT_TRUE(ctx, tsInput);
    if (!tsInput) return;
    tsInput.value().programNumber = selectedTsProgram().programNumber;
    tsInput.value().programMapPid = selectedTsProgram().programMapPid;
    tsInput.value().videoPid = selectedTsProgram().videoPid;
    tsInput.value().audioPid = selectedTsProgram().audioPid;
    tsInput.value().pcrPid = selectedTsProgram().pcrPid;
    tsInput.value().maximumPcrGap27Mhz = 2'700'000;
    tsInput.value().projectionCapacity =
        tsInput.value().evidenceTimelineCapacity;
    tsInput.value().timestampTimeBaseNumerator = 1;
    tsInput.value().timestampTimeBaseDenominator = 90'000;
    tsInput.value().initialSourceGeneration = MediaFirstLockedSourceGeneration;
    tsInput.value().videoPacketDuration = MediaTsPacketDurationEvidence{
        outer.videoPlan.sourceStreamIndex,
        static_cast<std::uint16_t>(selectedTsProgram().videoPid),
        3'003, MediaRational{1, 90'000}};
    tsInput.value().audioPacketDuration = MediaTsPacketDurationEvidence{
        outer.audioPlan.sourceStreamIndex,
        static_cast<std::uint16_t>(selectedTsProgram().audioPid),
        1'024, MediaRational{1, 48'000}};
    outer.input.mpegTs = std::move(tsInput).value();

    const auto resolvedOutput = validTsResolvedFacts();
    auto synchronization = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selectedTsProgram(), &resolvedOutput);
    EXPECT_TRUE(ctx, synchronization);
    if (!synchronization) return;
    auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
        outer, std::move(synchronization).value());
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    outer.avSyncRuntime = std::move(runtime).value();

    EXPECT_TRUE(ctx,
                MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    const auto& planned = *outer.avSyncRuntime;
    EXPECT_EQ(ctx, planned.outputAdapter,
              MediaAvSyncOutputAdapterKind::ProjectMpegTs);
    EXPECT_EQ(ctx, planned.transition.participants.size(), std::size_t{4});
    EXPECT_EQ(ctx, planned.transition.participants.back().participant,
              MediaAvGenerationParticipant::ProjectMpegTsOutput);
    EXPECT_TRUE(ctx,
                std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
                    planned.protocolOutput));
    auto& protocol = std::get<MediaProjectMpegTsRuntimeOutputPlan>(
        outer.avSyncRuntime->protocolOutput);
    protocol.url.clear();
    EXPECT_FALSE(ctx,
                 MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                     outer));
    protocol.url = outer.muxedOutput.url;
    auto changedMuxParameters = protocol.protocol.muxPlan().parameters();
    changedMuxParameters.tableVersion = 1;
    auto changedMux = MediaTsMuxPlan::create(changedMuxParameters);
    EXPECT_TRUE(ctx, changedMux);
    auto changedProtocol = MediaProjectMpegTsOutputPlan::accept(
        protocol.protocol.audioSampleRate(), std::move(changedMux).value());
    EXPECT_TRUE(ctx, changedProtocol);
    protocol.protocol = std::move(changedProtocol).value();
    EXPECT_FALSE(ctx,
                 MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
                     outer));
    protocol.protocol = MediaProjectMpegTsOutputPlan::accept(
        protocol.protocol.audioSampleRate(),
        *outer.avSyncRuntime->synchronization.ts->outputMux).value();
    const auto rejectChangedMux = [&](const char* name, auto mutation) {
        auto parameters =
            outer.avSyncRuntime->synchronization.ts->outputMux->parameters();
        mutation(parameters);
        auto mux = MediaTsMuxPlan::create(std::move(parameters));
        if (!mux) return;
        auto accepted = MediaProjectMpegTsOutputPlan::accept(
            protocol.protocol.audioSampleRate(), std::move(mux).value());
        EXPECT_TRUE(ctx, accepted);
        if (!accepted) return;
        protocol.protocol = std::move(accepted).value();
        if (MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer)) {
            std::cerr << "expected TS runtime mux mismatch: " << name << '\n';
        }
        EXPECT_FALSE(ctx,
            MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
        protocol.protocol = MediaProjectMpegTsOutputPlan::accept(
            outer.avSyncRuntime->audioCorrection.outputSampleRate,
            *outer.avSyncRuntime->synchronization.ts->outputMux).value();
    };
    rejectChangedMux("transportStreamId", [](auto& p) { p.transportStreamId = 2; });
    rejectChangedMux("programNumber", [](auto& p) { p.programNumber = 2; });
    rejectChangedMux("programMapPid", [](auto& p) { p.programMapPid = 0x0110; });
    rejectChangedMux("videoPid", [](auto& p) { p.videoPid = p.pcrPid = 0x0111; });
    rejectChangedMux("audioPid", [](auto& p) { p.audioPid = 0x0112; });
    rejectChangedMux("pcrPid", [](auto& p) { p.pcrPid = p.audioPid; });
    rejectChangedMux("tableVersion", [](auto& p) { p.tableVersion = 1; });
    rejectChangedMux("psiRepeatInterval", [](auto& p) {
        p.psiRepeatInterval = MediaRunningTime::fromNanoseconds(50'000'000);
    });
    rejectChangedMux("h264InputLayout", [](auto& p) {
        p.h264InputLayout = MediaTsH264InputLayout::AnnexB;
    });
    rejectChangedMux("h264NalLengthBytes", [](auto& p) { p.h264NalLengthBytes = 3; });
    rejectChangedMux("parameterSetPolicy", [](auto& p) {
        p.parameterSetPolicy = MediaTsParameterSetPolicy::Never;
    });
    rejectChangedMux("aac.mpegId", [](auto& p) { p.aac.mpegId = 1; });
    rejectChangedMux("aac.audioObjectType", [](auto& p) { p.aac.audioObjectType = 1; });
    rejectChangedMux("aac.samplingFrequencyIndex", [](auto& p) {
        p.aac.samplingFrequencyIndex = 4;
    });
    rejectChangedMux("aac.channelConfiguration", [](auto& p) {
        p.aac.channelConfiguration = 1;
    });
    rejectChangedMux("clock.pcrInterval", [](auto& p) {
        p.clock.pcrInterval = MediaRunningTime::fromNanoseconds(25'000'000);
    });
    rejectChangedMux("clock.maximumPcrGap", [](auto& p) {
        p.clock.maximumPcrGap = MediaRunningTime::fromNanoseconds(200'000'000);
    });
    rejectChangedMux("clock.maximumPcrJitter", [](auto& p) {
        p.clock.maximumPcrJitter = MediaRunningTime::fromNanoseconds(4'000'000);
    });
    rejectChangedMux("transportDecodeLead", [](auto& p) {
        p.transportDecodeLead = MediaRunningTime::fromNanoseconds(101'000'000);
    });
    rejectChangedMux("continuity.pat", [](auto& p) { p.continuity.pat = 1; });
    rejectChangedMux("continuity.pmt", [](auto& p) { p.continuity.pmt = 1; });
    rejectChangedMux("continuity.video", [](auto& p) { p.continuity.video = 1; });
    rejectChangedMux("continuity.audio", [](auto& p) { p.continuity.audio = 1; });
    rejectChangedMux("maximumPacketsPerDatagram", [](auto& p) {
        p.maximumPacketsPerDatagram = 6;
    });
    rejectChangedMux("maximumAudioAccessUnitSamples", [](auto& p) {
        p.maximumAudioAccessUnitSamples = 960;
    });

    protocol.protocol = MediaProjectMpegTsOutputPlan::accept(
        protocol.protocol.audioSampleRate() + 1,
        *outer.avSyncRuntime->synchronization.ts->outputMux).value();
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    protocol.protocol = MediaProjectMpegTsOutputPlan::accept(
        outer.avSyncRuntime->audioCorrection.outputSampleRate,
        *outer.avSyncRuntime->synchronization.ts->outputMux).value();
    protocol.url += "?mutated=1";
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    protocol.url = outer.muxedOutput.url;
    protocol.resourceKind = MediaOutputResourceKind::FFmpegFormatContext;
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    protocol.resourceKind = MediaOutputResourceKind::ByteSink;
    protocol.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    protocol.muxSessionKind = MediaMuxSessionKind::ProjectMpegTs;
    outer.muxedOutput.outputResourceKind = MediaOutputResourceKind::FFmpegFormatContext;
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    outer.muxedOutput.outputResourceKind = MediaOutputResourceKind::ByteSink;
    outer.muxedOutput.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    EXPECT_FALSE(ctx, MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(outer));
    outer.muxedOutput.muxSessionKind = MediaMuxSessionKind::ProjectMpegTs;
}

void testTsResolvedOutputSupportMatrix(TestContext& ctx)
{
    const auto audio = resolvedAacOutput(MediaAudioProfile::knownAacLow());
    const auto nonDefaultLead = MediaRunningTime::fromNanoseconds(37'000'000);
    const auto lengthPrefixed = MediaEncodedPacketLayout::lengthPrefixed(4);
    EXPECT_TRUE(ctx, lengthPrefixed);
    if (!lengthPrefixed) return;
    const auto valid = MediaProjectMpegTsOutputPlan::create(
        "h264", lengthPrefixed.value(), audio, nonDefaultLead);
    EXPECT_TRUE(ctx, valid);
    if (valid) {
        EXPECT_EQ(ctx, valid.value().audioSampleRate(), 48'000);
        EXPECT_EQ(ctx, valid.value().muxPlan().parameters().aac.audioObjectType,
                  std::uint8_t{2});
        EXPECT_EQ(ctx, valid.value().muxPlan().parameters().aac.samplingFrequencyIndex,
                  std::uint8_t{3});
        EXPECT_EQ(ctx, valid.value().muxPlan().parameters().aac.channelConfiguration,
                  std::uint8_t{2});
        EXPECT_EQ(ctx, valid.value().muxPlan().transportDecodeLead(), nonDefaultLead);
        EXPECT_EQ(ctx, valid.value().muxPlan().parameters().h264InputLayout,
                  MediaTsH264InputLayout::LengthPrefixed);
        EXPECT_EQ(ctx, valid.value().muxPlan().parameters().h264NalLengthBytes,
                  std::uint8_t{4});
    }
    const auto startCode = MediaProjectMpegTsOutputPlan::create(
        "h264", MediaEncodedPacketLayout::startCodeDelimited(), audio,
        nonDefaultLead);
    EXPECT_TRUE(ctx, startCode);
    if (startCode) {
        EXPECT_EQ(ctx, startCode.value().muxPlan().parameters().h264InputLayout,
                  MediaTsH264InputLayout::AnnexB);
    }
    const auto unsupportedWidth = MediaEncodedPacketLayout::lengthPrefixed(8);
    EXPECT_TRUE(ctx, unsupportedWidth);
    if (unsupportedWidth) {
        const auto rejected = MediaProjectMpegTsOutputPlan::create(
            "h264", unsupportedWidth.value(), audio, nonDefaultLead);
        EXPECT_FALSE(ctx, rejected);
        if (!rejected) {
            EXPECT_EQ(ctx, rejected.error().code,
                      ::media::ErrorCode::Unsupported);
        }
    }
    EXPECT_FALSE(ctx, MediaEncodedPacketLayout::lengthPrefixed(0));
    EXPECT_FALSE(ctx, MediaProjectMpegTsOutputPlan::create(
        "hevc", lengthPrefixed.value(), audio, nonDefaultLead));
    EXPECT_FALSE(ctx, MediaProjectMpegTsOutputPlan::create(
        "h264", lengthPrefixed.value(),
        resolvedAacOutput(MediaAudioProfile::unknown()), nonDefaultLead));
}

void testProjectTsRequiresSelectedEncoderPacketLayoutFact(TestContext& ctx)
{
    MediaPipelinePlan plan;
    plan.branchMode = MediaBranchMode::TranscodeFrame;
    plan.selected.encoder.ffmpegName = "h264_nvenc";
    const auto missing =
        MediaSelectedEncoderPacketLayoutResolver::resolve(plan);
    EXPECT_FALSE(ctx, missing);
    if (!missing) {
        EXPECT_EQ(ctx, missing.error().code, ::media::ErrorCode::Unsupported);
    }

    plan.selected.encoder.encodedPacketLayout =
        MediaEncodedPacketLayout::startCodeDelimited();
    const auto explicitLayout =
        MediaSelectedEncoderPacketLayoutResolver::resolve(plan);
    EXPECT_TRUE(ctx, explicitLayout);
    if (explicitLayout) {
        EXPECT_EQ(ctx, explicitLayout.value().kind(),
                  MediaEncodedPacketLayoutKind::StartCodeDelimited);
    }

    plan.branchMode = MediaBranchMode::CopyPacket;
    EXPECT_FALSE(ctx,
                 MediaSelectedEncoderPacketLayoutResolver::resolve(plan));
}

void testEncoderPacketLayoutCapabilityIsExactAndFailClosed(TestContext& ctx)
{
    for (const std::string_view encoderName : {
             "h264_nvenc", "libx264", "libx264rgb"}) {
        const auto layout =
            MediaEncoderPacketLayoutCapabilityProvider::find(encoderName);
        EXPECT_TRUE(ctx, layout.has_value());
        if (layout) {
            EXPECT_EQ(ctx, layout->kind(),
                      MediaEncodedPacketLayoutKind::StartCodeDelimited);
            EXPECT_FALSE(ctx, layout->lengthFieldBytes().has_value());
        }
    }

    for (const std::string_view encoderName : {
             "h264_nvenc_alias", "H264_nvenc", "libx264-custom", "h264_qsv"}) {
        EXPECT_FALSE(
            ctx, MediaEncoderPacketLayoutCapabilityProvider::find(encoderName));
    }
}

void testVideoCapabilityScannerPublishesOnlyProvenEncoderLayout(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "cuda";
    const auto candidates =
        MediaVideoCapabilityScanner::enumerateTranscodeCandidates("h264", "h264", options);

    const auto findEncoder = [&](std::string_view ffmpegName) -> const MediaPipelineStagePlan*
    {
        const auto candidate =
            std::find_if(candidates.begin(), candidates.end(),
                         [&](const auto& chain) { return chain.encoder.ffmpegName == ffmpegName; });
        return candidate == candidates.end() ? nullptr : &candidate->encoder;
    };

    const auto* nvenc = findEncoder("h264_nvenc");
    EXPECT_TRUE(ctx, nvenc != nullptr);
    if (nvenc)
    {
        EXPECT_TRUE(ctx, nvenc->encodedPacketLayout.has_value());
        if (nvenc->encodedPacketLayout)
        {
            EXPECT_EQ(ctx, nvenc->encodedPacketLayout->kind(),
                      MediaEncodedPacketLayoutKind::StartCodeDelimited);
        }
    }

    MediaPipelinePlannerOptions softwareOptions(false, true, true, true);
    softwareOptions.preferredHardware = "software";
    const auto softwareCandidates =
        MediaVideoCapabilityScanner::enumerateTranscodeCandidates("h264", "h264", softwareOptions);
    const auto softwareEncoder = std::find_if(
        softwareCandidates.begin(), softwareCandidates.end(),
        [](const auto& chain) { return chain.encoder.ffmpegName == "libx264"; });
    const auto* libx264 = softwareEncoder == softwareCandidates.end()
                             ? nullptr
                             : &softwareEncoder->encoder;
    EXPECT_TRUE(ctx, libx264 != nullptr);
    if (libx264)
        EXPECT_TRUE(ctx, libx264->encodedPacketLayout.has_value());

    const auto* qsv = findEncoder("h264_qsv");
    EXPECT_TRUE(ctx, qsv != nullptr);
    if (qsv)
        EXPECT_FALSE(ctx, qsv->encodedPacketLayout.has_value());
}

void testVideoCapabilityScannerHonorsDisableHardwareDecision(TestContext& ctx)
{
    MediaPipelinePlannerOptions softwareOnly(false, true, true, true);
    softwareOnly.preferredHardware = "software";
    const auto softwareCandidates =
        MediaVideoCapabilityScanner::enumerateTranscodeCandidates("h264", "h264", softwareOnly);

    EXPECT_FALSE(ctx, softwareCandidates.empty());
    EXPECT_TRUE(ctx, std::all_of(softwareCandidates.begin(), softwareCandidates.end(),
                                 [](const auto& chain)
                                 {
                                     return !chain.decoder.hardware && !chain.filter.hardware &&
                                            !chain.encoder.hardware;
                                 }));

    MediaPipelinePlannerOptions hardwareEnabled(false, true, false, true);
    hardwareEnabled.preferredHardware = "auto";
    const auto completeCandidates =
        MediaVideoCapabilityScanner::enumerateTranscodeCandidates("h264", "h264", hardwareEnabled);

    const auto cuda = std::find_if(
        completeCandidates.begin(), completeCandidates.end(),
        [](const auto& chain) { return chain.label == "cuda-nvenc"; });
    EXPECT_TRUE(ctx, cuda != completeCandidates.end());
    if (cuda != completeCandidates.end()) {
        EXPECT_EQ(ctx, cuda->decoder.ffmpegName, std::string("h264"));
        EXPECT_TRUE(ctx, cuda->decoder.hardware);
        EXPECT_EQ(ctx, cuda->decoder.hwaccelName, std::string("cuda"));
    }

    EXPECT_TRUE(ctx, std::any_of(completeCandidates.begin(), completeCandidates.end(),
                                 [](const auto& chain)
                                 {
                                     return chain.decoder.hardware || chain.filter.hardware ||
                                            chain.encoder.hardware;
                                 }));
    EXPECT_TRUE(ctx, std::none_of(completeCandidates.begin(), completeCandidates.end(),
                                 [](const auto& chain)
                                 {
                                     return !chain.decoder.hardware && !chain.filter.hardware &&
                                            !chain.encoder.hardware;
                                 }));

    MediaPipelineChainPlan hardware;
    hardware.label = "hardware";
    hardware.decoder.available = true;
    hardware.decoder.hardware = true;
    hardware.decoder.zeroCopy = true;
    hardware.decoder.deviceKind = MediaHardwareDeviceKind::CUDA;
    hardware.filter = hardware.decoder;
    hardware.encoder = hardware.decoder;

    MediaPipelineChainPlan software;
    software.label = "software";
    software.decoder.available = true;
    software.filter.available = true;
    software.encoder.available = true;

    const auto scored = MediaPipelineScorer::scoreAndSortChains(
        {std::move(software), std::move(hardware)}, hardwareEnabled);
    EXPECT_FALSE(ctx, scored.empty());
    if (!scored.empty())
    {
        EXPECT_EQ(ctx, scored.front().label, std::string("hardware"));
        EXPECT_TRUE(ctx, scored.front().score > scored.back().score);
    }
}

MediaPipelineChainPlan makeAvailableHardwareChain(std::string label,
                                                  MediaHardwareDeviceKind deviceKind,
                                                  std::string hwaccelName,
                                                  int declaredPriority)
{
    MediaPipelineChainPlan chain;
    chain.label = std::move(label);
    chain.decoder.available = true;
    chain.decoder.hardware = true;
    chain.decoder.zeroCopy = true;
    chain.decoder.deviceKind = deviceKind;
    chain.decoder.hwaccelName = hwaccelName;
    chain.decoder.priority = declaredPriority;
    chain.filter = chain.decoder;
    chain.filter.role = MediaPipelineStageRole::Filter;
    chain.encoder = chain.decoder;
    chain.encoder.role = MediaPipelineStageRole::Encoder;
    return chain;
}

void testPipelineScorerConsumesPriorityAndUsesStableTieBreaker(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "auto";

    auto lowerPriority = makeAvailableHardwareChain(
        "z-lower", MediaHardwareDeviceKind::CUDA, "cuda", 10);
    auto higherPriority = makeAvailableHardwareChain(
        "m-higher", MediaHardwareDeviceKind::QSV, "qsv", 20);
    auto alphabeticalTie = makeAvailableHardwareChain(
        "a-higher", MediaHardwareDeviceKind::D3D11VA, "d3d11va", 20);

    const auto scored = MediaPipelineScorer::scoreAndSortChains(
        {std::move(lowerPriority), std::move(higherPriority), std::move(alphabeticalTie)},
        options);

    EXPECT_EQ(ctx, scored.at(0).label, std::string("a-higher"));
    EXPECT_EQ(ctx, scored.at(1).label, std::string("m-higher"));
    EXPECT_EQ(ctx, scored.at(2).label, std::string("z-lower"));
    EXPECT_TRUE(ctx, scored.at(0).score > scored.at(2).score);
}

void testHardwareProbeValidatesCompleteChainOncePerCandidate(TestContext& ctx)
{
    int chainValidations = 0;
    MediaHardwareCapabilityProbe probe(
        [&](const MediaPipelineChainPlan& candidate,
            const MediaPipelinePlannerOptions& candidateOptions)
        {
            ++chainValidations;
            EXPECT_EQ(ctx, candidate.label, std::string("cuda"));
            EXPECT_EQ(ctx, candidate.decoder.deviceKind, MediaHardwareDeviceKind::CUDA);
            EXPECT_EQ(ctx, candidate.decoder.hwaccelName, std::string("cuda"));
            EXPECT_TRUE(ctx, candidateOptions.filterRequired);
            return MediaHardwareCapability{true, "test chain negotiated"};
        });
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "auto";
    auto chain = makeAvailableHardwareChain(
        "cuda", MediaHardwareDeviceKind::CUDA, "cuda", 10);

    EXPECT_TRUE(ctx, probe.validate(chain, options));
    EXPECT_EQ(ctx, chainValidations, 1);
}

void testPlannerPreflightsOnlyHighestRankedHardwareChain(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "auto";
    std::vector<std::string> attempted;
    MediaHardwareCapabilityProbe probe(
        [&](const MediaPipelineChainPlan& candidate,
            const MediaPipelinePlannerOptions&)
        {
            attempted.push_back(candidate.label);
            return MediaHardwareCapability{
                false,
                "test encoder open failed"};
        });

    auto candidates = MediaPipelineScorer::scoreAndSortChains(
        {makeAvailableHardwareChain("qsv", MediaHardwareDeviceKind::QSV, "qsv", 20),
         makeAvailableHardwareChain("cuda", MediaHardwareDeviceKind::CUDA, "cuda", 30),
         makeAvailableHardwareChain("d3d11va", MediaHardwareDeviceKind::D3D11VA,
                                    "d3d11va", 10)},
        options);
    const auto selected =
        MediaPipelinePlanner::selectHighestRankedCandidate(candidates, options);

    EXPECT_TRUE(ctx, selected);
    MediaPipelineChainPlan selectedChain;
    if (selected) {
        selectedChain = candidates.at(selected.value());
        EXPECT_EQ(ctx, selectedChain.label, std::string("cuda"));
        EXPECT_FALSE(ctx, MediaPipelinePlanner::preflightSelectedCandidate(
                              selectedChain, options, probe));
    }
    EXPECT_EQ(ctx, attempted.size(), std::size_t{1});
    if (attempted.size() == 1) {
        EXPECT_EQ(ctx, attempted.at(0), std::string("cuda"));
    }
}

void testPlannerRejectsImplicitSoftwareFallbackWhenHardwareFails(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "auto";
    int deviceCreations = 0;
    MediaHardwareCapabilityProbe probe(
        [&](const MediaPipelineChainPlan&,
            const MediaPipelinePlannerOptions&)
        {
            ++deviceCreations;
            return MediaHardwareCapability{false, "test chain unavailable"};
        });

    MediaPipelineChainPlan software;
    software.label = "software";
    software.decoder.available = true;
    software.filter.available = true;
    software.encoder.available = true;
    auto candidates = MediaPipelineScorer::scoreAndSortChains(
        {makeAvailableHardwareChain("cuda", MediaHardwareDeviceKind::CUDA, "cuda", 30),
         std::move(software)},
        options);
    const auto selected =
        MediaPipelinePlanner::selectHighestRankedCandidate(candidates, options);

    EXPECT_TRUE(ctx, selected);
    if (selected) {
        MediaPipelineChainPlan selectedChain = candidates.at(selected.value());
        EXPECT_EQ(ctx, selectedChain.label, std::string("cuda"));
        const auto preflight = MediaPipelinePlanner::preflightSelectedCandidate(
            selectedChain, options, probe);
        EXPECT_FALSE(ctx, preflight);
        if (!preflight) {
            EXPECT_EQ(ctx, preflight.error().code, ::media::ErrorCode::HardwareUnavailable);
        }
    }
    EXPECT_EQ(ctx, deviceCreations, 1);
}

void testPlannerUsesSoftwareOnlyWhenHardwareIsExplicitlyDisabled(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, true, true);
    options.preferredHardware = "software";
    int deviceCreations = 0;
    MediaHardwareCapabilityProbe probe(
        [&](const MediaPipelineChainPlan&,
            const MediaPipelinePlannerOptions&)
        {
            ++deviceCreations;
            return MediaHardwareCapability{true, "unexpected chain validation"};
        });

    MediaPipelineChainPlan software;
    software.label = "software";
    software.decoder.available = true;
    software.filter.available = true;
    software.encoder.available = true;
    auto candidates =
        MediaPipelineScorer::scoreAndSortChains({std::move(software)}, options);
    const auto selected =
        MediaPipelinePlanner::selectHighestRankedCandidate(candidates, options);

    EXPECT_TRUE(ctx, selected);
    if (selected) {
        MediaPipelineChainPlan selectedChain = candidates.at(selected.value());
        EXPECT_EQ(ctx, selectedChain.label, std::string("software"));
        EXPECT_TRUE(ctx, MediaPipelinePlanner::preflightSelectedCandidate(
                             selectedChain, options, probe));
    }
    EXPECT_EQ(ctx, deviceCreations, 0);
}

void testProjectTsPlannerConsumesPublishedLayoutOrRejectsUnknownEncoder(TestContext& ctx)
{
    MediaPipelinePlan videoPlan;
    videoPlan.branchMode = MediaBranchMode::TranscodeFrame;
    videoPlan.outputCodecName = "h264";
    videoPlan.selected.encoder.ffmpegName = "h264_nvenc";
    videoPlan.selected.encoder.encodedPacketLayout =
        MediaEncoderPacketLayoutCapabilityProvider::find("h264_nvenc");

    const auto published = MediaSelectedEncoderPacketLayoutResolver::resolve(videoPlan);
    EXPECT_TRUE(ctx, published);
    if (published)
    {
        const auto tsPlan = MediaProjectMpegTsOutputPlan::create(
            videoPlan.outputCodecName, published.value(),
            resolvedAacOutput(MediaAudioProfile::knownAacLow()),
            MediaRunningTime::fromNanoseconds(100'000'000));
        EXPECT_TRUE(ctx, tsPlan);
        if (tsPlan)
        {
            EXPECT_EQ(ctx, tsPlan.value().muxPlan().parameters().h264InputLayout,
                      MediaTsH264InputLayout::AnnexB);
        }
    }

    videoPlan.selected.encoder.ffmpegName = "h264_qsv";
    videoPlan.selected.encoder.encodedPacketLayout =
        MediaEncoderPacketLayoutCapabilityProvider::find("h264_qsv");
    const auto rejected = MediaSelectedEncoderPacketLayoutResolver::resolve(videoPlan);
    EXPECT_FALSE(ctx, rejected);
    if (!rejected)
    {
        EXPECT_EQ(ctx, rejected.error().code, ::media::ErrorCode::Unsupported);
    }
}

MediaTsMuxPlanParameters validTsMuxPlanParameters()
{
    return MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024};
}

void testTsMuxPlanRejectsEveryInvalidField(TestContext& ctx)
{
    EXPECT_TRUE(ctx, MediaTsMuxPlan::create(validTsMuxPlanParameters()));
    const auto reject = [&ctx](const char* name, auto mutation) {
        auto parameters = validTsMuxPlanParameters();
        mutation(parameters);
        const auto result = MediaTsMuxPlan::create(std::move(parameters));
        if (result) std::cerr << "expected invalid TS mux field: " << name << '\n';
        EXPECT_FALSE(ctx, result);
    };

    reject("transport stream id", [](auto& p) { p.transportStreamId = 0; });
    reject("program number", [](auto& p) { p.programNumber = 0; });
    reject("PAT PID", [](auto& p) { p.patPid = 1; });
    reject("PMT PID collision", [](auto& p) { p.programMapPid = p.videoPid; });
    reject("video PID collision", [](auto& p) { p.videoPid = p.audioPid; });
    reject("audio PID collision", [](auto& p) { p.audioPid = p.videoPid; });
    reject("PCR PID is not ES", [](auto& p) { p.pcrPid = 0x0103; });
    reject("table version", [](auto& p) { p.tableVersion = 32; });
    reject("PSI repeat positive", [](auto& p) {
        p.psiRepeatInterval = MediaRunningTime::fromNanoseconds(0);
    });
    reject("PSI after PCR interval", [](auto& p) { p.psiRepeatInterval = p.clock.pcrInterval; });
    reject("video stream type", [](auto& p) { p.videoStreamType = 0x24; });
    reject("audio stream type", [](auto& p) { p.audioStreamType = 0x11; });
    reject("H264 layout", [](auto& p) {
        p.h264InputLayout = static_cast<MediaTsH264InputLayout>(0xFF);
    });
    reject("NAL length zero", [](auto& p) { p.h264NalLengthBytes = 0; });
    reject("NAL length five", [](auto& p) { p.h264NalLengthBytes = 5; });
    reject("parameter set policy", [](auto& p) {
        p.parameterSetPolicy = static_cast<MediaTsParameterSetPolicy>(0xFF);
    });
    reject("AAC MPEG id", [](auto& p) { p.aac.mpegId = 2; });
    reject("AAC object type", [](auto& p) { p.aac.audioObjectType = 0; });
    reject("AAC sampling index", [](auto& p) { p.aac.samplingFrequencyIndex = 13; });
    reject("AAC channels", [](auto& p) { p.aac.channelConfiguration = 0; });
    reject("PCR interval positive", [](auto& p) {
        p.clock.pcrInterval = MediaRunningTime::fromNanoseconds(0);
    });
    reject("PCR gap ordered", [](auto& p) { p.clock.maximumPcrGap = p.clock.pcrInterval; });
    reject("PCR jitter ordered", [](auto& p) { p.clock.maximumPcrJitter = p.clock.pcrInterval; });
    reject("timestamp numerator", [](auto& p) { p.clock.timestampTimeBaseNumerator = 2; });
    reject("timestamp denominator", [](auto& p) { p.clock.timestampTimeBaseDenominator = 1'000; });
    reject("transport lead", [](auto& p) {
        p.transportDecodeLead = MediaRunningTime::fromNanoseconds(0);
    });
    reject("packet size", [](auto& p) { p.packetSize = 192; });
    reject("PAT continuity", [](auto& p) { p.continuity.pat = 16; });
    reject("PMT continuity", [](auto& p) { p.continuity.pmt = 16; });
    reject("video continuity", [](auto& p) { p.continuity.video = 16; });
    reject("audio continuity", [](auto& p) { p.continuity.audio = 16; });
    reject("datagram minimum", [](auto& p) { p.maximumPacketsPerDatagram = 0; });
    reject("datagram maximum", [](auto& p) { p.maximumPacketsPerDatagram = 8; });
    reject("transport kind", [](auto& p) {
        p.transportKind = static_cast<MediaTsOutputTransportKind>(0xFF);
    });
    reject("audio access-unit samples", [](auto& p) {
        p.maximumAudioAccessUnitSamples = 0;
    });
}

#include <array>

void testProjectTsOutputRequiresExplicitUdpEndpoint(TestContext& ctx)
{
    auto request = avSyncTsRequest();
    const auto accepts = [&](const char* url) {
        request.output.url = url;
        return MediaRealtimeOutputPolicyPlanner::planUrls(request);
    };
    EXPECT_TRUE(ctx, accepts("udp://127.0.0.1:5000"));
    EXPECT_FALSE(ctx, accepts("rtp://127.0.0.1:5000"));
    EXPECT_FALSE(ctx, accepts("output.ts"));
    EXPECT_FALSE(ctx, accepts("udp://:5000"));
    EXPECT_FALSE(ctx, accepts("udp://127.0.0.1"));
    EXPECT_FALSE(ctx, accepts("srt://127.0.0.1:5000"));
}

void testAvSyncPlannerBuildsCompleteRtpContract(TestContext& ctx)
{
    const auto request = avSyncRtpRequest();
    const auto result = MediaAvSyncPlanner::plan(request);
    EXPECT_TRUE(ctx, result);
    if (!result) return;

    const MediaAvSyncPlan& plan = result.value();
    EXPECT_EQ(ctx, *plan.topology, MediaAvSyncTopology::SeparateRtpToSeparateRtp);
    EXPECT_EQ(ctx, *plan.sourceClockMode, MediaAvSyncSourceClockMode::RtpSenderReports);
    EXPECT_EQ(ctx, *plan.canonicalTimeBaseNumerator, 1);
    EXPECT_EQ(ctx, *plan.canonicalTimeBaseDenominator, 1000000000);
    EXPECT_EQ(ctx, *plan.rtp->videoInput.clockRate, 90000);
    EXPECT_EQ(ctx, *plan.rtp->audioInput.clockRate, 48000);
    EXPECT_EQ(ctx, plan.rtp->input.streamAssociationMode,
              MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair);
    EXPECT_EQ(ctx, plan.rtp->input.rtcpCompositionMode,
              MediaRtcpCompositionMode::ReducedSizeRfc5506);
    EXPECT_EQ(ctx, plan.rtp->input.identityEvidenceTimeoutNs->nanoseconds(),
              9'000'000'000LL);
    EXPECT_TRUE(ctx, *plan.rtp->input.requireSenderReports);
    EXPECT_EQ(ctx, plan.rtp->input.senderReportTimeoutNs->nanoseconds(),
              7'000'000'000LL);
    EXPECT_EQ(ctx, plan.rtp->input.maximumExtrapolationNs->nanoseconds(),
              9'000'000'000LL);
    EXPECT_EQ(ctx, *plan.rtp->input.maximumSenderClockRateErrorPpm, 1000);
    EXPECT_EQ(ctx, plan.rtp->input.maximumSenderClockResidualNs->nanoseconds(),
              250'000'000);
    EXPECT_TRUE(ctx, *plan.rtp->videoOutput.ssrc != *plan.rtp->audioOutput.ssrc);
    EXPECT_EQ(ctx, *plan.rtp->videoOutput.cname, *plan.rtp->audioOutput.cname);
    EXPECT_TRUE(ctx, *plan.rtp->output.useSharedNtpEpoch);
    EXPECT_TRUE(ctx, plan.rtp->output.senderReportIntervalNs->nanoseconds() > 0);
    EXPECT_FALSE(ctx, plan.audioServo.commandLeadNs.has_value());
    EXPECT_FALSE(ctx, plan.audioServo.compensationWindowNs.has_value());
    EXPECT_FALSE(ctx, plan.audioServo.frequencyFilterTimeConstantNs.has_value());
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validatePolicy(plan));
    EXPECT_FALSE(ctx, MediaAvSyncPlanValidator::validate(plan));

    const auto rawRequest = completeAvSyncRtpRequest();
    const auto rawSynchronization = MediaAvSyncPlanner::plan(rawRequest);
    EXPECT_TRUE(ctx, rawSynchronization);
    if (!rawSynchronization) return;
    const auto raw = MediaRealtimeInputPlanner::planRawRtp(
        rawRequest, &rawSynchronization.value());
    EXPECT_TRUE(ctx, raw);
    if (raw && raw.value().audioTransport) {
        for (const auto* transport : {
                 &raw.value().videoTransport,
                 &*raw.value().audioTransport}) {
            EXPECT_TRUE(ctx, transport->requireSenderReports);
            EXPECT_FALSE(ctx, transport->requireCname);
            EXPECT_EQ(ctx, transport->senderReportTimeoutMs, 7'000);
            EXPECT_EQ(ctx, transport->cnameTimeoutMs, 9'000);
            EXPECT_EQ(ctx, transport->rtcpCompositionMode,
                      MediaRtcpCompositionMode::ReducedSizeRfc5506);
        }
    }

    auto excessiveCapacity = avSyncRtpRequest();
    excessiveCapacity.parameters.queues.packet = 257;
    EXPECT_FALSE(ctx, MediaAvSyncPlanner::plan(excessiveCapacity));
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

    auto missingAvSync = completeAvSyncRtpRequest();
    const auto missingAvSyncResult =
        MediaRealtimeInputPlanner::planRawRtp(missingAvSync, nullptr);
    EXPECT_FALSE(ctx, missingAvSyncResult);
    if (!missingAvSyncResult) {
        EXPECT_EQ(ctx, missingAvSyncResult.error().code,
                  ::media::ErrorCode::InvalidArgument);
    }

    const auto raw = MediaRealtimeInputPlanner::planRawRtp(request, nullptr);
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
    EXPECT_FALSE(ctx, transport.requireCname);
    EXPECT_EQ(ctx, transport.senderReportTimeoutMs, 7'000);
    EXPECT_EQ(ctx, transport.cnameTimeoutMs, 9'000);
    EXPECT_EQ(ctx, transport.rtcpCompositionMode, MediaRtcpCompositionMode::ReducedSizeRfc5506);

    auto ipv6Request = request;
    ipv6Request.input.videoRtp.url = "rtp://[::1]:5004";
    const auto ipv6Raw = MediaRealtimeInputPlanner::planRawRtp(ipv6Request, nullptr);
    EXPECT_TRUE(ctx, ipv6Raw);
    if (ipv6Raw) {
        EXPECT_EQ(ctx, ipv6Raw.value().videoTransport.addressFamily, MediaIpAddressFamily::Ipv6);
        EXPECT_EQ(ctx, ipv6Raw.value().videoTransport.bindAddress, std::string("::1"));
        EXPECT_TRUE(ctx, ipv6Raw.value().videoSdp.empty());
        EXPECT_EQ(ctx, ipv6Raw.value().videoDepacketizer.codecName, std::string("h264"));
    }

    auto multicastV4 = request;
    multicastV4.input.videoRtp.url = "rtp://239.1.2.3:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(multicastV4, nullptr));
    auto zeroNetwork = request;
    zeroNetwork.input.videoRtp.url = "rtp://0.1.2.3:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(zeroNetwork, nullptr));
    auto reservedHigh = request;
    reservedHigh.input.videoRtp.url = "rtp://240.0.0.1:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(reservedHigh, nullptr));
    auto reserved255 = request;
    reserved255.input.videoRtp.url = "rtp://255.0.0.1:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(reserved255, nullptr));
    auto loopback = request;
    loopback.input.videoRtp.url = "rtp://127.0.0.1:5004";
    EXPECT_TRUE(ctx, MediaRealtimeInputPlanner::planRawRtp(loopback, nullptr));
    auto multicastV6 = request;
    multicastV6.input.videoRtp.url = "rtp://[ff02::1]:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(multicastV6, nullptr));
    auto hostname = request;
    hostname.input.videoRtp.url = "rtp://localhost:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(hostname, nullptr));
    auto familyMismatch = request;
    familyMismatch.input.videoRtp.url = "rtp://[127.0.0.1]:5004";
    EXPECT_FALSE(ctx, MediaRealtimeInputPlanner::planRawRtp(familyMismatch, nullptr));
}

void testAvSyncPlannerBuildsCompleteTsContract(TestContext& ctx)
{
    const auto resolvedOutput = validTsResolvedFacts();
    const auto result = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selectedTsProgram(), &resolvedOutput);
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
    EXPECT_TRUE(ctx, plan.ts->outputMux.has_value());
    if (!plan.ts->outputMux) return;
    const auto& output = plan.ts->outputMux->parameters();
    EXPECT_EQ(ctx, output.transportStreamId, std::uint16_t{1});
    EXPECT_EQ(ctx, output.programNumber, std::uint16_t{1});
    EXPECT_EQ(ctx, output.programMapPid, std::uint16_t{0x0100});
    EXPECT_EQ(ctx, output.videoPid, std::uint16_t{0x0101});
    EXPECT_EQ(ctx, output.audioPid, std::uint16_t{0x0102});
    EXPECT_EQ(ctx, output.pcrPid, std::uint16_t{0x0101});
    EXPECT_EQ(ctx, plan.ts->outputMux->transportDecodeLead(), *plan.startup.outputLeadNs);
    EXPECT_FALSE(ctx, plan.audioServo.commandLeadNs.has_value());
    EXPECT_FALSE(ctx, plan.audioServo.compensationWindowNs.has_value());
    EXPECT_FALSE(ctx, plan.audioServo.frequencyFilterTimeConstantNs.has_value());
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validatePolicy(plan));
    EXPECT_FALSE(ctx, MediaAvSyncPlanValidator::validate(plan));
}

void testAvSyncPlannerRejectsSeparateRtpToTs(TestContext& ctx)
{
    auto request = avSyncRtpRequest();
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    const auto result = MediaAvSyncPlanner::plan(request);
    EXPECT_FALSE(ctx, result);
    if (!result) EXPECT_EQ(ctx, result.error().code, media::ErrorCode::Unsupported);
}

MediaAvSyncPlan finalizedAvSyncTestPlan(MediaAvSyncPlan plan)
{
    plan.audioServo.commandLeadNs =
        MediaRunningTime::fromNanoseconds(1'500'000'000);
    plan.audioServo.compensationWindowNs =
        MediaRunningTime::fromNanoseconds(2'000'000'000);
    plan.audioServo.frequencyFilterTimeConstantNs =
        MediaRunningTime::fromNanoseconds(5'000'000'000);
    return plan;
}

void testAvSyncValidatorRejectsMissingAndInconsistentFields(TestContext& ctx)
{
    const auto result = MediaAvSyncPlanner::plan(avSyncRtpRequest());
    EXPECT_TRUE(ctx, result);
    if (!result) return;
    const MediaAvSyncPlan complete = finalizedAvSyncTestPlan(result.value());

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
    EXPECT_MISSING(startup.videoCapacity);
    EXPECT_MISSING(startup.audioCapacity);
    EXPECT_MISSING(startup.videoByteCapacity);
    EXPECT_MISSING(startup.audioByteCapacity);
    EXPECT_MISSING(startup.maximumVideoUnitBytes);
    EXPECT_MISSING(startup.maximumAudioUnitBytes);
    EXPECT_MISSING(startup.videoIdentity);
    EXPECT_MISSING(startup.audioIdentity);
    EXPECT_MISSING(startup.allowDegradedClock);
    EXPECT_MISSING(audioServo.deadbandNs);
    EXPECT_MISSING(audioServo.phaseFilterTimeConstantNs);
    EXPECT_MISSING(audioServo.frequencyFilterTimeConstantNs);
    EXPECT_MISSING(audioServo.proportionalGainPpmPerSecond);
    EXPECT_MISSING(audioServo.integralGainPpmPerSecondSquared);
    EXPECT_MISSING(audioServo.integratorLimitPpm);
    EXPECT_MISSING(audioServo.frequencyFeedForwardNumerator);
    EXPECT_MISSING(audioServo.frequencyFeedForwardDenominator);
    EXPECT_MISSING(audioServo.frequencyDeadbandPpm);
    EXPECT_MISSING(audioServo.maximumMeasuredFrequencyPpm);
    EXPECT_MISSING(audioServo.recoveryExitFrequencyPpm);
    EXPECT_MISSING(audioServo.antiWindupMode);
    EXPECT_MISSING(audioServo.minimumUpdateIntervalNs);
    EXPECT_MISSING(audioServo.maximumMeasurementGapNs);
    EXPECT_MISSING(audioServo.maximumSlewPpmPerSecond);
    EXPECT_MISSING(audioServo.normalCorrectionLimitPpm);
    EXPECT_MISSING(audioServo.recoveryCorrectionLimitPpm);
    EXPECT_MISSING(audioServo.recoveryEnterThresholdNs);
    EXPECT_MISSING(audioServo.recoveryExitThresholdNs);
    EXPECT_MISSING(audioServo.recoveryExitHoldNs);
    EXPECT_MISSING(audioServo.compensationWindowNs);
    EXPECT_MISSING(audioServo.commandLeadNs);
    EXPECT_MISSING(audioServo.outputSampleRate);
    EXPECT_MISSING(audioServo.correctionLookaheadWindows);
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
    {
        MediaAvSyncPlan missing = complete;
        missing.rtp->input.streamAssociationMode =
            MediaAvSyncRtpStreamAssociationMode::Unknown;
        expectInvalid(std::move(missing));
    }
    EXPECT_MISSING(rtp->input.rtcpCompositionMode);
    EXPECT_MISSING(rtp->input.identityEvidenceTimeoutNs);
    EXPECT_MISSING(rtp->input.requireSenderReports);
    EXPECT_MISSING(rtp->input.senderReportTimeoutNs);
    EXPECT_MISSING(rtp->input.maximumExtrapolationNs);
    EXPECT_MISSING(rtp->input.maximumInterStreamClockOffsetSkewNs);
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

    const auto resolvedOutput = validTsResolvedFacts();
    const auto tsResult = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selectedTsProgram(), &resolvedOutput);
    EXPECT_TRUE(ctx, tsResult);
    if (!tsResult) return;
    const MediaAvSyncPlan completeTs = finalizedAvSyncTestPlan(tsResult.value());
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
    EXPECT_MISSING_TS(outputMux);
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
    const auto resolvedOutput = validTsResolvedFacts();
    const auto result = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selectedTsProgram(), &resolvedOutput);
    EXPECT_TRUE(ctx, result);
    if (!result) return;
    const MediaAvSyncPlan complete = finalizedAvSyncTestPlan(result.value());
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
    const auto resolvedOutput = validTsResolvedFacts();
    const auto tsResult = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selectedTsProgram(), &resolvedOutput);
    EXPECT_TRUE(ctx, rtpResult);
    EXPECT_TRUE(ctx, tsResult);
    if (!rtpResult || !tsResult) return;

    const MediaAvSyncPlan rtp = finalizedAvSyncTestPlan(rtpResult.value());
    auto rejectRtp = [&](const char* name, auto mutation) {
        MediaAvSyncPlan plan = rtp;
        mutation(plan);
        expectInvalidAvSyncMutation(ctx, std::move(plan), name);
    };
    rejectRtp("startup wait positive", [](auto& p) { p.startup.maximumWaitNs = avSyncTime(0); });
    rejectRtp("startup preroll positive", [](auto& p) { p.startup.prerollNs = avSyncTime(-1); });
    rejectRtp("startup skew within output lead", [](auto& p) { p.startup.maximumInitialSkewNs = *p.startup.outputLeadNs; });
    rejectRtp("startup video capacity positive", [](auto& p) { p.startup.videoCapacity = 0; });
    rejectRtp("startup audio capacity positive", [](auto& p) { p.startup.audioCapacity = 0; });
    rejectRtp("startup capacity representable", [](auto& p) { p.startup.videoCapacity = std::numeric_limits<std::size_t>::max(); });
    rejectRtp("startup video capacity bounded", [](auto& p) {
        p.startup.videoCapacity = 257;
        p.startup.videoByteCapacity = 257 * *p.startup.maximumVideoUnitBytes;
    });
    rejectRtp("startup audio capacity bounded", [](auto& p) {
        p.startup.audioCapacity = 257;
        p.startup.audioByteCapacity = 257 * *p.startup.maximumAudioUnitBytes;
    });
    rejectRtp("startup byte capacity matches units", [](auto& p) { ++*p.startup.videoByteCapacity; });
    rejectRtp("startup unit byte maximum positive", [](auto& p) { p.startup.maximumVideoUnitBytes = 0; });
    rejectRtp("startup byte capacity serializable", [](auto& p) {
        p.startup.maximumVideoUnitBytes = static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()) + 1;
        p.startup.videoByteCapacity = p.startup.maximumVideoUnitBytes;
        p.startup.videoCapacity = 1;
    });
    rejectRtp("startup degraded clock denied", [](auto& p) { p.startup.allowDegradedClock = true; });
    rejectRtp("startup identities distinct", [](auto& p) { p.startup.audioIdentity = p.startup.videoIdentity; });
    rejectRtp("audio deadband positive", [](auto& p) { p.audioServo.deadbandNs = avSyncTime(0); });
    rejectRtp("audio filter after minimum update", [](auto& p) { p.audioServo.phaseFilterTimeConstantNs = *p.audioServo.minimumUpdateIntervalNs; });
    rejectRtp("audio measurement gap after filter", [](auto& p) { p.audioServo.maximumMeasurementGapNs = *p.audioServo.phaseFilterTimeConstantNs; });
    rejectRtp("audio estimator after measurement gap", [](auto& p) { p.audioServo.frequencyFilterTimeConstantNs = *p.audioServo.maximumMeasurementGapNs; });
    rejectRtp("audio command lead after measurement gap", [](auto& p) { p.audioServo.commandLeadNs = *p.audioServo.maximumMeasurementGapNs; });
    rejectRtp("audio command lead covers positive correction gap", [](auto& p) {
        p.audioServo.maximumMeasurementGapNs = avSyncTime(1'000'000'000);
        p.audioServo.commandLeadNs = avSyncTime(1'001'000'000);
        p.audioServo.compensationWindowNs = avSyncTime(1'500'000'000);
        p.audioServo.frequencyFilterTimeConstantNs = avSyncTime(3'000'000'000);
        p.audioServo.outputSampleRate = 48'000;
        p.audioServo.recoveryCorrectionLimitPpm = 5'000;
    });
    rejectRtp("audio compensation window after command lead", [](auto& p) { p.audioServo.compensationWindowNs = *p.audioServo.commandLeadNs; });
    rejectRtp("audio estimator after compensation window", [](auto& p) { p.audioServo.compensationWindowNs = *p.audioServo.frequencyFilterTimeConstantNs; });
    rejectRtp("audio output sample rate positive", [](auto& p) { p.audioServo.outputSampleRate = 0; });
    rejectRtp("audio output sample rate bounded", [](auto& p) { p.audioServo.outputSampleRate = 384'001; });
    rejectRtp("audio correction lookahead positive", [](auto& p) { p.audioServo.correctionLookaheadWindows = 0; });
    rejectRtp("audio correction lookahead bounded", [](auto& p) { p.audioServo.correctionLookaheadWindows = 9; });
    rejectRtp("audio measurement gap bounded", [](auto& p) { p.audioServo.maximumMeasurementGapNs = avSyncTime(1'000'000'001); });
    rejectRtp("audio recovery exit after deadband", [](auto& p) { p.audioServo.recoveryExitThresholdNs = *p.audioServo.deadbandNs; });
    rejectRtp("audio recovery entry after exit", [](auto& p) { p.audioServo.recoveryEnterThresholdNs = *p.audioServo.recoveryExitThresholdNs; });
    rejectRtp("audio recovery before hard discontinuity", [](auto& p) { p.audioServo.recoveryEnterThresholdNs = *p.recovery.hardDiscontinuityThresholdNs; });
    rejectRtp("audio frequency deadband below exit", [](auto& p) { p.audioServo.frequencyDeadbandPpm = *p.audioServo.recoveryExitFrequencyPpm; });
    rejectRtp("audio exit frequency below measured maximum", [](auto& p) { p.audioServo.recoveryExitFrequencyPpm = *p.audioServo.maximumMeasuredFrequencyPpm; });
    rejectRtp("audio integrator within recovery correction", [](auto& p) { p.audioServo.integratorLimitPpm = *p.audioServo.recoveryCorrectionLimitPpm + 1; });
    rejectRtp("audio recovery not below normal", [](auto& p) { p.audioServo.recoveryCorrectionLimitPpm = *p.audioServo.normalCorrectionLimitPpm - 1; });
    rejectRtp("audio slew within normal correction", [](auto& p) { p.audioServo.maximumSlewPpmPerSecond = *p.audioServo.normalCorrectionLimitPpm + 1; });
    rejectRtp("audio hard threshold after recovery", [](auto& p) { p.recovery.hardDiscontinuityThresholdNs = *p.recovery.suspectThresholdNs; });
    rejectRtp("video hold positive", [](auto& p) { p.video.earlyHoldThresholdNs = avSyncTime(0); });
    rejectRtp("video late after hold", [](auto& p) { p.video.lateDisplayThresholdNs = *p.video.earlyHoldThresholdNs; });
    rejectRtp("video drop after late", [](auto& p) { p.video.dropThresholdNs = *p.video.lateDisplayThresholdNs; });
    rejectRtp("video recovery count positive", [](auto& p) { p.video.maximumConsecutiveRecoveryActions = 0; });
    rejectRtp("discontinuity after video recovery", [](auto& p) { p.recovery.suspectThresholdNs = *p.video.dropThresholdNs; });
    rejectRtp("reacquire after hard threshold", [](auto& p) { p.recovery.reacquisitionTimeoutNs = *p.recovery.hardDiscontinuityThresholdNs; });

    MediaAvSyncPlan exactLeadBoundary = rtp;
    exactLeadBoundary.audioServo.maximumMeasurementGapNs = avSyncTime(1'000'000'000);
    exactLeadBoundary.audioServo.commandLeadNs = avSyncTime(1'005'020'834);
    exactLeadBoundary.audioServo.compensationWindowNs = avSyncTime(1'500'000'000);
    exactLeadBoundary.audioServo.frequencyFilterTimeConstantNs = avSyncTime(3'000'000'000);
    exactLeadBoundary.audioServo.outputSampleRate = 48'000;
    exactLeadBoundary.audioServo.recoveryCorrectionLimitPpm = 5'000;
    EXPECT_TRUE(ctx, MediaAvSyncPlanValidator::validate(exactLeadBoundary));
    rejectRtp("metrics p95 positive", [](auto& p) { p.metrics.maximumSteadyP95SkewNs = avSyncTime(0); });
    rejectRtp("metrics p99 after p95", [](auto& p) { p.metrics.maximumSteadyP99SkewNs = avSyncTime(1); });
    rejectRtp("metrics startup accepts p99", [](auto& p) { p.metrics.maximumStartupSkewNs = avSyncTime(1); });
    rejectRtp("RTP SR interval positive", [](auto& p) { p.rtp->output.senderReportIntervalNs = avSyncTime(0); });
    rejectRtp("RTP SR interval before timeout", [](auto& p) { p.rtp->output.senderReportIntervalNs = *p.rtp->input.senderReportTimeoutNs; });
    rejectRtp("RTP timeout within extrapolation", [](auto& p) { p.rtp->input.senderReportTimeoutNs = *p.rtp->input.maximumExtrapolationNs; });
    rejectRtp("RTP extrapolation before reacquire", [](auto& p) { p.rtp->input.maximumExtrapolationNs = *p.recovery.reacquisitionTimeoutNs; });
    rejectRtp("RTP clock offset skew positive", [](auto& p) {
        p.rtp->input.maximumInterStreamClockOffsetSkewNs = avSyncTime(0);
    });
    rejectRtp("RTP clock offset skew below hard discontinuity", [](auto& p) {
        p.rtp->input.maximumInterStreamClockOffsetSkewNs =
            *p.recovery.hardDiscontinuityThresholdNs;
    });
    rejectRtp("RTP sender clock rate error positive", [](auto& p) { p.rtp->input.maximumSenderClockRateErrorPpm = 0; });
    rejectRtp("RTP sender clock residual positive", [](auto& p) { p.rtp->input.maximumSenderClockResidualNs = avSyncTime(0); });

}

void testResolvedAudioPlannerMatrix(TestContext& ctx)
{
    auto source = [](std::string codec, MediaAudioProfile profile, int rate, int channels) {
        MediaInputAudioStreamInfo value;
        value.streamIndex = 1;
        value.codecName = std::move(codec);
        value.profile = std::move(profile);
        value.sampleRate = rate;
        value.channels = channels;
        value.channelLayout = channels == 1 ? "mono" : "stereo";
        value.sampleFormat = "fltp";
        value.bitrateBitsPerSecond = 128'000;
        value.maximumAccessUnitSamples = 1024;
        return value;
    };
    const auto he = MediaAudioProfile::fromCodecProfile("aac", "HE-AAC");
    const auto heV2 = MediaAudioProfile::fromCodecProfile("aac", "HE-AAC-v2");
    EXPECT_TRUE(ctx, he);
    EXPECT_TRUE(ctx, heV2);
    if (!he || !heV2) return;

    MediaAudioPipelinePlannerOptions ordinary(true);
    auto lcCopy = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2), ordinary);
    EXPECT_TRUE(ctx, lcCopy);
    if (lcCopy) EXPECT_EQ(ctx, lcCopy.value().branchMode, MediaBranchMode::CopyPacket);

    auto synchronized = ordinary;
    synchronized.outputRequirement.requireFrameTranscode = true;
    auto lcSynchronized = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2),
        synchronized);
    EXPECT_TRUE(ctx, lcSynchronized);
    if (lcSynchronized) {
        EXPECT_EQ(ctx, lcSynchronized.value().branchMode,
                  MediaBranchMode::TranscodeFrame);
    }

    auto unknownCopy = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::unknown(), 48'000, 2), ordinary);
    EXPECT_TRUE(ctx, unknownCopy);
    if (unknownCopy && unknownCopy.value().resolvedOutput) {
        EXPECT_EQ(ctx, unknownCopy.value().branchMode, MediaBranchMode::CopyPacket);
        EXPECT_EQ(ctx, unknownCopy.value().resolvedOutput->profile().knowledge(),
                  MediaAudioProfileKnowledge::Unknown);
    }
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", he.value(), 48'000, 2), ordinary));
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", heV2.value(), 48'000, 2), ordinary));
    auto explicitHeWithoutTopology = ordinary;
    explicitHeWithoutTopology.requestedProfile = "aac_he";
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2),
        explicitHeWithoutTopology));
    auto explicitHeV2WithoutTopology = ordinary;
    explicitHeV2WithoutTopology.requestedProfile = "aac_he_v2";
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2),
        explicitHeV2WithoutTopology));

    auto mismatchedExplicitProfile = ordinary;
    mismatchedExplicitProfile.requestedProfile = "aac_low";
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("opus", MediaAudioProfile::notApplicable(), 48'000, 2),
        mismatchedExplicitProfile));

    auto unavailableCodecCopy = MediaAudioPipelinePlanner::planKnownAudio(
        source("codec_without_encoder", MediaAudioProfile::notApplicable(), 48'000, 2),
        ordinary);
    EXPECT_TRUE(ctx, unavailableCodecCopy);
    if (unavailableCodecCopy) {
        EXPECT_EQ(ctx, unavailableCodecCopy.value().branchMode, MediaBranchMode::CopyPacket);
    }
    auto unsupportedAacRate = ordinary;
    unsupportedAacRate.requestedSampleRate = 12'345;
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2),
        unsupportedAacRate));

    MediaAudioPipelinePlannerOptions ts(true);
    ts.outputRequirement.codecName = "aac";
    ts.outputRequirement.profile = MediaAudioProfile::knownAacLow();
    ts.outputRequirement.sampleRate = 48'000;
    ts.outputRequirement.channels = 2;
    auto heToLc = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", he.value(), 48'000, 2), ts);
    EXPECT_TRUE(ctx, heToLc);
    if (heToLc) EXPECT_EQ(ctx, heToLc.value().branchMode, MediaBranchMode::TranscodeFrame);
    auto unknownToLc = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::unknown(), 48'000, 2), ts);
    EXPECT_TRUE(ctx, unknownToLc);
    if (unknownToLc) EXPECT_EQ(ctx, unknownToLc.value().branchMode, MediaBranchMode::TranscodeFrame);
    auto mono441 = MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 44'100, 1), ts);
    EXPECT_TRUE(ctx, mono441);
    if (mono441 && mono441.value().resolvedOutput) {
        EXPECT_EQ(ctx, mono441.value().resolvedOutput->sampleRate(), 48'000);
        EXPECT_EQ(ctx, mono441.value().resolvedOutput->channels(), 2);
    }
    auto opusToAac = MediaAudioPipelinePlanner::planKnownAudio(
        source("opus", MediaAudioProfile::notApplicable(), 48'000, 2), ts);
    EXPECT_TRUE(ctx, opusToAac);
    if (opusToAac && opusToAac.value().resolvedOutput) {
        EXPECT_EQ(ctx, opusToAac.value().resolvedOutput->codecName(), std::string("aac"));
        EXPECT_EQ(ctx, opusToAac.value().resolvedOutput->profile().canonicalName(), std::string("aac_low"));
    }

    auto explicitHe = ts;
    explicitHe.requestedProfile = "aac_he";
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", he.value(), 48'000, 2), explicitHe));
    auto explicitRate = ts;
    explicitRate.requestedSampleRate = 44'100;
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2), explicitRate));
    auto explicitMono = ts;
    explicitMono.requestedChannels = 1;
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("aac", MediaAudioProfile::knownAacLow(), 48'000, 2), explicitMono));
    auto explicitOpus = ts;
    explicitOpus.requestedCodecName = "opus";
    EXPECT_FALSE(ctx, MediaAudioPipelinePlanner::planKnownAudio(
        source("opus", MediaAudioProfile::notApplicable(), 48'000, 2), explicitOpus));

    MediaResolvedAudioSource encoderSource{
        "aac", MediaAudioProfile::knownAacLow(), 44'100, 2, "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest encoderRequest;
    encoderRequest.sampleRate = 48'000;
    const auto encoderTarget = MediaResolvedAudioTargetDecision::create(
        encoderSource, encoderRequest, {});
    EXPECT_TRUE(ctx, encoderTarget);
    if (!encoderTarget) return;
    EXPECT_FALSE(ctx, MediaResolvedAudioOutputPlan::create(
        encoderTarget.value(),
        MediaSelectedAudioEncoder{"aac", "fltp", {44'100}, {}}, std::nullopt));
    EXPECT_FALSE(ctx, MediaResolvedAudioOutputPlan::create(
        encoderTarget.value(),
        MediaSelectedAudioEncoder{"aac", "fltp", {48'000}, {AV_PROFILE_AAC_HE}}, std::nullopt));
    EXPECT_FALSE(ctx, MediaResolvedAudioOutputPlan::create(
        encoderTarget.value(),
        MediaSelectedAudioEncoder{"aac", "fltp", {48'000}, {}}, std::nullopt));

    MediaResolvedAudioSource aacWithoutProfile{
        "aac", MediaAudioProfile::notApplicable(), 48'000, 2, "stereo", "fltp", 128'000};
    EXPECT_FALSE(ctx, MediaResolvedAudioTargetDecision::create(
        aacWithoutProfile, {}, {}));
    MediaResolvedAudioSource opusWithAacProfile{
        "opus", MediaAudioProfile::knownAacLow(), 48'000, 2, "stereo", "fltp", 128'000};
    EXPECT_FALSE(ctx, MediaResolvedAudioTargetDecision::create(
        opusWithAacProfile, {}, {}));

    MediaResolvedAudioSource unknownAacSource{
        "aac", MediaAudioProfile::unknown(), 48'000, 2, "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest unknownTranscodeRequest;
    unknownTranscodeRequest.sampleRate = 44'100;
    EXPECT_FALSE(ctx, MediaResolvedAudioTargetDecision::create(
        unknownAacSource, unknownTranscodeRequest, {}));

    const auto copyTarget = MediaResolvedAudioTargetDecision::create(
        encoderSource, {}, {});
    EXPECT_TRUE(ctx, copyTarget);
    if (copyTarget) {
        EXPECT_FALSE(ctx, MediaResolvedAudioOutputPlan::create(
            copyTarget.value(),
            MediaSelectedAudioEncoder{
                "aac", "fltp", {48'000}, {AV_PROFILE_AAC_LOW}}, 1024));
    }
}

void testAudioEncoderTargetIdentityValidator(TestContext& ctx)
{
    MediaResolvedAudioSource source{
        "aac", MediaAudioProfile::knownAacLow(), 48'000, 2,
        "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest request;
    request.sampleRate = 44'100;
    const auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    EXPECT_TRUE(ctx, target);
    if (!target) return;

    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, context != nullptr);
    if (!context) return;
    context->sample_rate = target.value().sampleRate();
    context->sample_fmt = AV_SAMPLE_FMT_FLTP;
    context->profile = target.value().profile().ffmpegProfileId();
    context->time_base = AVRational{2, target.value().sampleRate() * 2};
    EXPECT_TRUE(ctx, av_channel_layout_from_string(
                         &context->ch_layout,
                         target.value().channelLayout().c_str()) >= 0);
    EXPECT_TRUE(ctx, MediaAudioEncoderTargetIdentityValidator::validate(
                         target.value(), AV_SAMPLE_FMT_FLTP, *context));

    av_channel_layout_uninit(&context->ch_layout);
    const AVChannelLayout differentTwoChannelLayout = AV_CHANNEL_LAYOUT_STEREO_DOWNMIX;
    EXPECT_TRUE(ctx, av_channel_layout_copy(
                         &context->ch_layout, &differentTwoChannelLayout) >= 0);
    EXPECT_FALSE(ctx, MediaAudioEncoderTargetIdentityValidator::validate(
                          target.value(), AV_SAMPLE_FMT_FLTP, *context));

    av_channel_layout_uninit(&context->ch_layout);
    EXPECT_TRUE(ctx, av_channel_layout_from_string(
                         &context->ch_layout,
                         target.value().channelLayout().c_str()) >= 0);
    context->time_base = AVRational{1, 48'000};
    EXPECT_FALSE(ctx, MediaAudioEncoderTargetIdentityValidator::validate(
                          target.value(), AV_SAMPLE_FMT_FLTP, *context));
}

} // namespace

int main()
{
    TestContext ctx;

    testTsInputPlanValidatorRejectsEveryMutation(ctx);
    testTsMuxPlanRejectsEveryInvalidField(ctx);
    testTsResolvedOutputSupportMatrix(ctx);
    testProjectTsRequiresSelectedEncoderPacketLayoutFact(ctx);
    testEncoderPacketLayoutCapabilityIsExactAndFailClosed(ctx);
    testVideoCapabilityScannerPublishesOnlyProvenEncoderLayout(ctx);
    testVideoCapabilityScannerHonorsDisableHardwareDecision(ctx);
    testPipelineScorerConsumesPriorityAndUsesStableTieBreaker(ctx);
    testHardwareProbeValidatesCompleteChainOncePerCandidate(ctx);
    testPlannerPreflightsOnlyHighestRankedHardwareChain(ctx);
    testPlannerRejectsImplicitSoftwareFallbackWhenHardwareFails(ctx);
    testPlannerUsesSoftwareOnlyWhenHardwareIsExplicitlyDisabled(ctx);
    testProjectTsPlannerConsumesPublishedLayoutOrRejectsUnknownEncoder(ctx);
    testResolvedAudioPlannerMatrix(ctx);
    testAudioEncoderTargetIdentityValidator(ctx);
    testProjectTsOutputRequiresExplicitUdpEndpoint(ctx);
    testTsProgramSelectorRequiresOneCrossValidatedProgram(ctx);
    testTsProgramSelectorRejectsAmbiguityAndInventoryMismatch(ctx);
    testTsEvidenceCapacityCoversProbeRollbackAndPredecessor(ctx);
    testAvSyncPlannerBuildsCompleteRtpContract(ctx);
    testRealtimePlannerProducesCompleteAvSyncRuntimeProduct(ctx);
    testPlannedProductAndRuntimeRejectSynchronizedAudioPacketCopy(ctx);
    testDecoderDelayUsesSelectedOutputSampleDomain(ctx);
    testRealtimePlannerProducesCompleteTsAvSyncRuntimeProduct(ctx);
    testSelectedResamplerPublishesSteadyStateBound(ctx);
    testAacAdtsDecoderCapabilityDoesNotRequireContainerExtradata(ctx);
    testOpusRtpDecoderCapabilityUsesPlannedProtocolBound(ctx);
    testRealtimeAvSyncRuntimeProductRejectsIndependentMutations(ctx);
    runAvSyncProductionPlanTests(ctx);
    testRawRtpInputPlannerProducesCompleteTransportPolicy(ctx);
    testAvSyncPlannerBuildsCompleteTsContract(ctx);
    testAvSyncPlannerRejectsSeparateRtpToTs(ctx);
    testAvSyncValidatorRejectsMissingAndInconsistentFields(ctx);
    testAvSyncValidatorRejectsProtocolIdentifierBoundaries(ctx);
    testAvSyncValidatorRejectsIsolatedNumericAndOrderingInvariants(ctx);
    MediaInputAudioStreamInfo source;
    source.streamIndex = 0;
    source.codecName = "aac";
    source.profile = MediaAudioProfile::knownAacLow();
    source.sampleRate = 48000;
    source.channels = 2;
    source.channelLayout = "stereo";
    source.sampleFormat = "fltp";
    source.bitrateBitsPerSecond = 320000;
    source.maximumAccessUnitSamples = 1024;

    MediaAudioPipelinePlannerOptions copyOptions(true);
    copyOptions.requestedCodecName = "aac";
    copyOptions.requestedSampleRate = 48000;
    copyOptions.requestedChannels = 2;
    copyOptions.requestedBitrateKbps = 320;
    const auto copy = MediaAudioPipelinePlanner::planKnownAudio(source, copyOptions);
    EXPECT_TRUE(ctx, copy);
    if (copy) {
        EXPECT_EQ(ctx, copy.value().branchMode, MediaBranchMode::CopyPacket);
        EXPECT_TRUE(ctx, copy.value().resolvedOutput.has_value());
        if (copy.value().resolvedOutput) {
            EXPECT_EQ(ctx, copy.value().resolvedOutput->profile().knowledge(),
                      MediaAudioProfileKnowledge::Known);
            EXPECT_EQ(ctx, copy.value().resolvedOutput->profile().canonicalName(),
                      std::string("aac_low"));
            EXPECT_TRUE(ctx, copy.value().resolvedOutput->encoderName().empty());
        }
    }

    auto transcodeOptions = copyOptions;
    transcodeOptions.requestedSampleRate = 44100;
    const auto transcode = MediaAudioPipelinePlanner::planKnownAudio(source, transcodeOptions);
    EXPECT_TRUE(ctx, transcode);
    if (transcode) {
        EXPECT_EQ(ctx, transcode.value().branchMode, MediaBranchMode::TranscodeFrame);
        EXPECT_TRUE(ctx, transcode.value().resolvedOutput.has_value());
        if (transcode.value().resolvedOutput) {
            EXPECT_EQ(ctx, transcode.value().resolvedOutput->sampleRate(), 44100);
            EXPECT_FALSE(ctx, transcode.value().resolvedOutput->encoderName().empty());
        }
    }

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
