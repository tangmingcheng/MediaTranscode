#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "application/realtime/MediaRealtimeCompositionPreparer.h"
#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "application/realtime/MediaRealtimeCompositionInputPreparer.h"
#include "application/realtime/MediaRealtimeCompositionPreparationState.h"
#include "application/realtime/MediaRealtimeCompositionSourcePreparer.h"
#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvOutputSynchronizationPlanner.h"
#include "internal/graph/planner/capability/MediaVideoOutputPreparer.h"
#include "internal/graph/planner/video/MediaVideoOutputPlanner.h"
#include "internal/graph/planner/video/MediaVideoColorContractPlanner.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceAudioPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAudioPlannerOptionsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoParametersPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeScheduledPacketizationPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanner.h"
#include "internal/graph/planner/realtime/MediaVideoOutputPreparationPlanner.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasPreparationPlanner.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedVideoCanvas.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"

extern "C" {
#include <libavutil/hwcontext.h>
}
#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using PreparedResult = ::media::Result<MediaRealtimeCompositionPrepared>;

struct PreparedCandidate final {
    MediaProbedVideoOutput output;
    MediaVideoCanvasGeometryPlan geometry;
    std::vector<MediaRealtimeCompositionPreparedSourceVideo> sources;
};

::media::Result<PreparedCandidate> prepareCandidate(
    const std::vector<MediaRealtimeCompositionSourceFacts>& facts,
    std::vector<MediaRealtimeCompositionCapturedSource>& captures,
    MediaPipelineStagePlan candidate, const MediaVideoTranscodeParameters& video,
    const MediaEncoderRateControlRequest& rateControl,
    MediaRational cadence,
    const MediaPreparationControl& control,
    const MediaSharedPreparationStorage& storageScope)
{
    using Result = ::media::Result<PreparedCandidate>;
    const MediaSize size{*video.width, *video.height};
    auto open = MediaVideoOutputPlanner::planOpenContract(candidate, size, cadence,
        rateControl, video, true);
    if (!open) return Result::failure(open.error());
    auto device = MediaVideoOutputPreparer::openDevice(candidate, control);
    if (!device) return Result::failure(device.error());
    if (!candidate.inputFrame) return Result::failure(::media::ErrorInfo::notInitialized(
        "Composition candidate has no frame geometry"));
    std::vector<MediaInputVideoStreamInfo> geometrySources;
    for (const auto& source : facts) geometrySources.push_back(source.raw.video);
    auto geometry = MediaVideoCanvasGeometryPlanner::plan(geometrySources,
        *candidate.inputFrame, facts.front().color);
    if (!geometry) return Result::failure(geometry.error());
    std::vector<MediaRealtimeCompositionPreparedSourceVideo> sources;
    for (std::size_t i = 0; i < facts.size(); ++i) {
        auto target = *candidate.inputFrame;
        target.size = {geometry.value().geometry.tiles[i].width, geometry.value().geometry.tiles[i].height};
        auto ranked = MediaPipelinePlanner::planVideoSourceCandidates(facts[i].raw.video,
            {{facts[i].raw.video.width, facts[i].raw.video.height}, target.size,
             facts[i].timing, true, cadence}, target);
        if (!ranked) return Result::failure(ranked.error());
        std::optional<::media::ErrorInfo> failure;
        bool prepared = false;
        for (auto& source : ranked.value()) {
            if (!source.available) continue;
            if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
                return Result::failure(status.error());
            const auto sourceLabel = source.label;
            auto selected = MediaRealtimeCompositionSourcePreparer::prepare(captures[i],
                facts[i], std::move(source), target, device.value().get(), control.cancellation, storageScope);
            if (!selected) {
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::PlannerCapability,
                    "composition_source_candidate_rejected source=" + std::to_string(i) +
                    " candidate=" + sourceLabel + " error=" + selected.error().describe());
                if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
                    return Result::failure(status.error());
                if (!failure) failure = selected.error();
                if (control.cancellation.stop_requested() ||
                    selected.error().code == ::media::ErrorCode::AllocationFailed)
                    return Result::failure(selected.error());
                continue;
            }
            if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
                return Result::failure(status.error());
            if (selected.value().evidence.firstFrame->color_range != facts[i].color.range)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Decoded source color range contradicts its complete SPS"));
            sources.push_back(std::move(selected).value());
            prepared = true;
            break;
        }
        if (!prepared) return Result::failure(failure ? *failure : ::media::ErrorInfo::hardwareUnavailable(
            "No ranked source candidate supports the actual shared composition device"));
    }
    std::vector<const AVFrame*> decodedFrames, tiles;
    std::vector<MediaVideoColorRangeFact> signaling;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        decodedFrames.push_back(sources[i].evidence.firstFrame.get());
        tiles.push_back(sources[i].tile.get());
        signaling.push_back(facts[i].color);
    }
    auto color = MediaVideoColorContractPlanner::plan(decodedFrames, tiles, signaling);
    if (!color) return Result::failure(color.error());
    const MediaVideoEncoderFrameInput frameInput{geometry.value().sampleAspectRatio,
        color.value().range, color.value().primaries, color.value().transfer, color.value().matrix};
    auto output = MediaVideoOutputPreparer::probe(std::move(candidate), std::move(open).value(),
        frameInput, std::move(device).value(), control);
    if (!output) return Result::failure(output.error());
    geometry.value().geometry.effectiveColorRange = *output.value().plan.encoder.effectiveColorRange;
    return Result::success({std::move(output).value(), std::move(geometry).value(), std::move(sources)});
}
} // namespace

PreparedResult MediaRealtimeCompositionPreparer::prepare(
    const MediaRealtimeRtpTranscodeRequest& request, MediaRealtimeVideoRunControl& runControl)
{
    if (!request.composition || request.composition->grid != MediaRealtimeCompositionGrid::Fixed ||
        request.composition->inputs.size() < 2 ||
        request.composition->inputs.size() > 4 ||
        request.composition->audioSource >= request.composition->inputs.size() ||
        request.parameters.execution.streamSet != MediaTranscodeStreamSet::AudioVideo ||
        !request.output.streamLayout || !request.output.transport ||
        !request.parameters.video.width || !request.parameters.video.height ||
        !request.parameters.video.frameRate.complete())
        return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
            "Composition requires complete grid, selected audio, output geometry and cadence"));
    auto plannedInputs = MediaRealtimeCompositionInputPlanner::plan(*request.composition);
    if (!plannedInputs) return PreparedResult::failure(plannedInputs.error());
    auto preparationBudget = MediaPreparationStorageBudget::create(
        plannedInputs.value().storageLimitBytes, MediaStandalonePreparationStorage{}, MediaStorageReleaseUnobserved{});
    if (!preparationBudget) return PreparedResult::failure(preparationBudget.error());
    const MediaSharedPreparationStorage storageScope{preparationBudget.value()};
    auto masterClock = std::make_shared<MediaSteadyMasterClock>(MediaRunningTime::fromNanoseconds(0));
    auto captured = MediaRealtimeCompositionInputPreparer::prepare(
        request, plannedInputs.value().inputs, masterClock, runControl.stopToken(), storageScope);
    if (!captured) return PreparedResult::failure(captured.error());
    auto captures = std::move(captured).value();
    auto deadline = captures.front().deadline;
    std::vector<MediaRealtimeCompositionSourceFacts> facts;
    std::vector<std::shared_ptr<MediaPreparedRtpSourceSession>> rtpSources;
    for (std::size_t i = 0; i < captures.size(); ++i) {
        deadline = (std::min)(deadline, captures[i].deadline);
        facts.push_back(std::move(captures[i].facts));
    }
    const MediaPreparationControl control{deadline, runControl.stopToken()};
    const auto selected = request.composition->audioSource;
    for (const auto& source : facts) {
        if (source.color.range != facts[selected].color.range)
            return PreparedResult::failure(::media::ErrorInfo::unsupported(
                "Composition sources require matching authoritative color ranges until a conversion contract is planned"));
    }
    auto audioOptions = MediaRealtimeAudioPlannerOptionsResolver::resolve(request);
    if (!audioOptions) return PreparedResult::failure(audioOptions.error());
    if (!facts[selected].raw.audio) return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
        "Selected composition audio source has no actual audio stream"));
    auto audio = MediaAudioPipelinePlanner::planKnownAudio(*facts[selected].raw.audio, audioOptions.value());
    if (!audio) return PreparedResult::failure(audio.error());
    if (!audio.value().resolvedOutput || !audio.value().preparedEmission)
        return PreparedResult::failure(::media::ErrorInfo::notInitialized(
            "Composition audio output lacks the unique prepared encoder readback"));
    auto planningResource = MediaPreparationMemoryResource::create(preparationBudget.value());
    if (!planningResource) return PreparedResult::failure(planningResource.error());
    auto sourceAudio = MediaCompositionSourceAudioPlanner::prepare(
        facts, selected, audio.value(), *planningResource.value(), control);
    if (!sourceAudio) return PreparedResult::failure(sourceAudio.error());
    if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
        return PreparedResult::failure(status.error());
    auto videoParameters = MediaRealtimeVideoParametersPlanner::plan(
        request.parameters.video, request.output.streamLayout);
    if (!videoParameters) return PreparedResult::failure(videoParameters.error());
    auto video = std::move(videoParameters).value();
    const MediaRational cadence{*video.frameRate.numerator, *video.frameRate.denominator};
    auto rateControl = MediaEncoderRateControlRequest::create(video.rateControl,
        video.bitrateKbps, video.minBitrateKbps, video.maxBitrateKbps);
    if (!rateControl) return PreparedResult::failure(rateControl.error());
    auto candidates = MediaVideoOutputPlanner::planCandidates(video.codecName,
        {*video.width, *video.height});
    std::optional<PreparedCandidate> prepared;
    std::optional<::media::ErrorInfo> failure;
    for (auto& candidate : candidates) {
        if (!candidate.available) continue;
        if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
            return PreparedResult::failure(status.error());
        const auto encoderName = candidate.ffmpegName;
        auto result = prepareCandidate(facts, captures, std::move(candidate), video,
            rateControl.value(), cadence, control, storageScope);
        if (result) {
            if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
                return PreparedResult::failure(status.error());
            prepared.emplace(std::move(result).value());
            break;
        }
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::PlannerCapability,
            "composition_output_candidate_rejected encoder=" + encoderName +
            " error=" + result.error().describe());
        if (auto status = MediaRealtimeCompositionPreparationState::validateInputs(captures, control); !status)
            return PreparedResult::failure(status.error());
        if (!failure) failure = result.error();
        if (result.error().code == ::media::ErrorCode::AllocationFailed)
            return PreparedResult::failure(result.error());
    }
    if (!prepared) return PreparedResult::failure(failure ? *failure : ::media::ErrorInfo::hardwareUnavailable(
        "No common prepared composition output/source capability"));
    for (const auto& capture : captures) {
        if (!capture.session) return PreparedResult::failure(::media::ErrorInfo::notInitialized(
            "Composition source preparation did not install its continuous owner"));
        rtpSources.push_back(capture.session);
    }
    auto emission = MediaPreparedEmissionResolver::resolveOutput(prepared->output.plan, cadence, &audio.value());
    if (!emission) return PreparedResult::failure(emission.error());
    auto base = MediaRealtimeDeploymentPlanner::planBase(request, emission.value());
    if (!base) return PreparedResult::failure(base.error());
    auto deployment = MediaRealtimeDeploymentPlanner::complete(std::move(base).value());
    if (!deployment) return PreparedResult::failure(deployment.error());
    auto urls = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    if (!urls) return PreparedResult::failure(urls.error());
    MediaRealtimeOutputPlanningDraft outputDraft;
    if (auto status = MediaRealtimeOutputPolicyPlanner::applyOutput(request, urls.value(), video,
            prepared->output.plan, &audio.value(), deployment.value(), outputDraft); !status)
        return PreparedResult::failure(status.error());
    const auto& encoder = prepared->output.plan.encoder;
    if (!encoder.encodedPacketLayout) return PreparedResult::failure(::media::ErrorInfo::notInitialized(
        "Composition prepared encoder has no encoded packet layout"));
    MediaProjectMpegTsResolvedPipelineFacts mpegTs{encoder.codecName,
        *encoder.encodedPacketLayout, *audio.value().resolvedOutput};
    auto outputSync = MediaAvOutputSynchronizationPlanner::plan({request.mediaId,
        request.output.streamLayout, request.output.transport, video.frameRate,
        audio.value().resolvedOutput->sampleRate(), deployment.value(),
        *request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream ? &mpegTs : nullptr});
    if (!outputSync) return PreparedResult::failure(outputSync.error());
    if (!outputSync.value().startup.outputLeadNs)
        return PreparedResult::failure(::media::ErrorInfo::notInitialized("Composition output lead is missing"));
    if (*request.output.streamLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        if (auto status = MediaRealtimeScheduledPacketizationPlanner::planAudioVideo(
                prepared->output.plan, audio.value(), outputDraft, outputSync.value()); !status)
            return PreparedResult::failure(status.error());
    }
    const auto& outputFrame = *encoder.inputFrame;
    auto outputLedger = MediaRealtimeGraphResourceLedgerPlanner::plan(deployment.value().encode().latency,
        emission.value(), {{outputFrame.size.width, outputFrame.size.height,
            outputFrame.surfacePixelFormat.empty() ? outputFrame.pixelFormat : outputFrame.surfacePixelFormat,
            cadence, "prepared-composition-output-frame-contract"}}, encoder.hardware());
    if (!outputLedger) return PreparedResult::failure(outputLedger.error());
    auto timing = MediaRealtimeAvSyncPlanningFactsResolver::resolveOutput(
        *audio.value().resolvedOutput, outputDraft, outputSync.value());
    if (!timing) return PreparedResult::failure(timing.error());
    const MediaAvSyncGroupKey outputGroupKey(request.mediaId + ".output");
    const MediaRealtimeAvOutputRuntimeRequest outputRequest{outputGroupKey,
        *audio.value().resolvedOutput, outputLedger.value(), std::nullopt, timing.value(),
        audio.value().resolvedOutput->codecFrameSamples(),
        *request.output.streamLayout, *request.output.transport, cadence, deployment.value(), emission.value()};
    MediaAvSyncPlan outputSynchronization = std::move(outputSync).value();
    auto output = MediaRealtimeAvOutputRuntimePlanner::plan(outputRequest,
        std::move(outputSynchronization), outputDraft);
    if (!output) return PreparedResult::failure(output.error());
    // Reserve output and canvas storage in the same root before the source
    // freeze snapshot. These owners survive until their real consumers take
    // their leases; a source solver must still account for its full peak costs.
    if (!prepared->output.plan.encoder.preparedEmission)
        return PreparedResult::failure(::media::ErrorInfo::notInitialized(
            "Early canvas retention lacks prepared output encoder emission"));
    auto plannedRetention = MediaVideoCanvasRetentionPlanner::plan(
        output.value().edgePolicies.preparedVideoFrame,
        *prepared->output.plan.encoder.preparedEmission);
    if (!plannedRetention) return PreparedResult::failure(plannedRetention.error());
    auto outputPreparation = MediaVideoOutputPreparationPlanner::plan(
        plannedRetention.value(), prepared->output.allocation);
    if (!outputPreparation) return PreparedResult::failure(outputPreparation.error());
    auto plannedCanvas = MediaVideoCanvasPreparationPlanner::plan(
        prepared->geometry.geometry, plannedRetention.value(), prepared->output.allocation);
    if (!plannedCanvas) return PreparedResult::failure(plannedCanvas.error());
    auto canvasBytes = MediaCheckedArithmetic::add(plannedCanvas.value().retainedPayloadBytes(),
        plannedCanvas.value().transientPayloadBytes(), "composition prepared canvas storage");
    if (!canvasBytes || canvasBytes.value() > (std::numeric_limits<std::size_t>::max)())
        return PreparedResult::failure(canvasBytes ? ::media::ErrorInfo::invalidArgument(
            "Canvas preparation exceeds platform storage size") : canvasBytes.error());
    auto outputCredit = storageScope.root->reserve(outputPreparation.value().storageBytes(), MediaStorageAdmission::RequiredAllocation);
    if (!outputCredit) return PreparedResult::failure(outputCredit.error());
    auto outputBudget = MediaPreparationStorageBudget::fromReservation(std::move(outputCredit).value(), MediaStorageReleaseUnobserved{});
    if (!outputBudget) return PreparedResult::failure(outputBudget.error());
    auto canvasCredit = storageScope.root->reserve(static_cast<std::size_t>(canvasBytes.value()), MediaStorageAdmission::RequiredAllocation);
    if (!canvasCredit) return PreparedResult::failure(canvasCredit.error());
    auto canvasBudget = MediaPreparationStorageBudget::fromReservation(std::move(canvasCredit).value(), MediaStorageReleaseUnobserved{});
    if (!canvasBudget) return PreparedResult::failure(canvasBudget.error());
    // Exact, budgeted and stable owner addresses for the later locked solver.
    // All borrowed source/video/deployment owners remain stable through bind.
    MediaFixedCapacityStorage<MediaCompositionSourceFixedProducts> fixedSources(
        captures.size(), *planningResource.value());
    for (std::size_t i = 0; i < captures.size(); ++i) {
        auto ingress = captures[i].capture.continuousIngress();
        if (!ingress) return PreparedResult::failure(ingress.error());
        const auto& session = rtpSources[i]->plan();
        auto fixed = MediaCompositionSourcePlanningKernel::prepareFixed({
            facts[i].request, facts[i].raw, facts[i].timing, ingress.value().video, ingress.value().audio,
            session.audioRetention ? &*session.audioRetention : nullptr,
            prepared->sources[i].plan, *audio.value().resolvedOutput, sourceAudio.value()[i],
            deployment.value(), output.value().activationOutputLead, i == selected}, planningResource.value());
        if (!fixed) return PreparedResult::failure(fixed.error());
        fixedSources.push_back(std::move(fixed).value());
    }
    if (auto frozen = MediaPreparedRtpSourceSession::freezeBatch(rtpSources, nullptr); !frozen)
        return PreparedResult::failure(frozen.error());
    std::vector<MediaPreparedRtpSource> sealed;
    std::vector<MediaRealtimeCompositionSourcePlan> sourcePlans;
    for (std::size_t i = 0; i < captures.size(); ++i) {
        auto source = std::move(captures[i].capture).seal();
        if (!source) return PreparedResult::failure(source.error());
        sealed.push_back(std::move(source).value());
        auto view = sealed.back().resources();
        if (!view) return PreparedResult::failure(view.error());
        auto plan = MediaRealtimeCompositionSourcePlanner::plan(
            std::move(fixedSources[i]), view.value(), emission.value());
        if (!plan) return PreparedResult::failure(plan.error());
        sourcePlans.push_back(std::move(plan).value());
    }
    std::vector<MediaAvAggregateSourceFacts> aggregateSources;
    aggregateSources.reserve(sourcePlans.size());
    for(const auto& source:sourcePlans){
        const auto& runtime=source.runtime;
        aggregateSources.push_back({runtime.groupKey,runtime.synchronization.members,runtime.synchronization.domainRole,
            runtime.synchronization.startup.videoReadiness,std::holds_alternative<MediaDecodedVideoAssemblyTiming>(runtime.assembly.video.timing),
            source.frameRate,runtime.queues.frame,runtime.activationOutputLead,runtime.assembly.initialGeneration,
            runtime.assembly.video.sourceIdentity,runtime.assembly.audio?std::optional<std::string_view>{runtime.assembly.audio->sourceIdentity}:std::nullopt,
            runtime.audioPipeline.enabled,runtime.audioPipeline.resolvedOutput?&*runtime.audioPipeline.resolvedOutput:nullptr,
            runtime.audioCorrection?&*runtime.audioCorrection:nullptr});
    }
    auto aggregate = MediaAvContinuousAggregatePlanner::plan(aggregateSources, output.value(),
        prepared->geometry.geometry, cadence, selected);
    if (!aggregate) return PreparedResult::failure(aggregate.error());
    auto topology = MediaRealtimeCompositionGraphBuilder::buildTopology(request.mediaId,
        {std::move(sourcePlans), prepared->output.plan, std::move(output).value(), std::move(aggregate).value()}, planningResource.value());
    if (!topology) return PreparedResult::failure(topology.error());
    auto sourceResources = topology.value().planSourceResources();
    if (!sourceResources) return PreparedResult::failure(sourceResources.error());
    auto retention = topology.value().planCanvasRetention();
    if (!retention) return PreparedResult::failure(retention.error());
    if (retention.value() != plannedRetention.value())
        return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
            "Production canvas retention differs from its early storage admission"));
    auto finalOutputPreparation = topology.value().planOutputPreparation(prepared->output.allocation);
    if (!finalOutputPreparation) return PreparedResult::failure(finalOutputPreparation.error());
    if (finalOutputPreparation.value().pool() != outputPreparation.value().pool() ||
        finalOutputPreparation.value().storageBytes() != outputPreparation.value().storageBytes())
        return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
            "Production output preparation differs from its early storage admission"));
    auto actualEncoder = MediaVideoOutputPreparer::prepare(prepared->output,
        outputPreparation.value(), outputBudget.value(), control);
    if (!actualEncoder) return PreparedResult::failure(actualEncoder.error());
    auto* frames = actualEncoder.value().context->hw_frames_ctx;
    if (!frames) return PreparedResult::failure(::media::ErrorInfo::notInitialized("Prepared output encoder has no actual canvas pool"));
    auto allocationFrame = ::media::ffmpeg::makeFrame();
    if (!allocationFrame) return PreparedResult::failure(::media::ErrorInfo::allocationFailed("Canvas allocation readback frame"));
    const int allocated = av_hwframe_get_buffer(frames, allocationFrame.get(), 0);
    if (allocated < 0) return PreparedResult::failure(FFmpegGraphError::fromCode(allocated, "canvas allocation readback"));
    auto allocation = readMediaVideoCanvasAllocation(*allocationFrame);
    if (!allocation) return PreparedResult::failure(allocation.error());
    if (allocation.value().surfaceBytes > outputPreparation.value().allocation().surfaceBytes ||
        allocation.value().stagingBytes > outputPreparation.value().allocation().stagingBytes)
        return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
            "Production canvas allocation exceeds the admitted same-device probe readback"));
    allocationFrame.reset();
    auto canvasPlan = topology.value().planCanvasPreparation(allocation.value());
    if (!canvasPlan) return PreparedResult::failure(canvasPlan.error());
    if (canvasPlan.value().retainedPayloadBytes() > plannedCanvas.value().retainedPayloadBytes() ||
        canvasPlan.value().transientPayloadBytes() > plannedCanvas.value().transientPayloadBytes())
        return PreparedResult::failure(::media::ErrorInfo::invalidArgument(
            "Production canvas storage exceeds its early storage admission"));
    std::vector<MediaVideoCanvasTile> tiles;
    for (const auto& source : prepared->sources) tiles.push_back({source.tile.get(), source.tileReadiness ? &*source.tileReadiness : nullptr});
    auto canvas = MediaPreparedVideoCanvas::prepare(canvasPlan.value(), frames, tiles, canvasBudget.value(), control);
    if (!canvas) return PreparedResult::failure(canvas.error());
    std::vector<std::shared_ptr<MediaPreparedVideoDecoder>> decoders;
    for (std::size_t i = 0; i < prepared->sources.size(); ++i) {
        if (!prepared->sources[i].tileReadiness) return PreparedResult::failure(::media::ErrorInfo::notInitialized("Prepared tile readiness missing"));
        if (auto completed = prepared->sources[i].tileReadiness->ensureCompleted(); !completed)
            return PreparedResult::failure(completed.error());
        prepared->sources[i].tile.reset();
        prepared->sources[i].tileReadiness.reset();
        const auto* sourceSnapshot = prepared->sources[i].descriptor->inputStreamSnapshot(facts[i].raw.video.streamIndex);
        if (!sourceSnapshot) return PreparedResult::failure(::media::ErrorInfo::notInitialized("Prepared decoder replay snapshot is missing"));
        auto decoder = MediaRtpSourceFirstFrameProbe::prepareReplay(std::move(prepared->sources[i].evidence),
            *sourceSnapshot, prepared->sources[i].plan, control);
        if (!decoder) return PreparedResult::failure(decoder.error());
        decoders.push_back(std::move(decoder).value());
    }
    MediaPreparedRealtimeEmissionSet actualOutputEmission{
        actualEncoder.value().emission, emission.value().audio,
        emission.value().audioFrames, emission.value().hardwareMemory};
    auto wrappedEncoder = FFmpegBufferFactory::wrapCodecContext(std::move(actualEncoder.value().context));
    if (!wrappedEncoder) return PreparedResult::failure(wrappedEncoder.error());
    auto bound = MediaRealtimeCompositionGraphBuilder::bind(std::move(topology).value(), deployment.value().encode().latency,
        {std::move(decoders), std::move(wrappedEncoder).value(), std::move(canvas).value(),
         canvasPlan.value().canvas().storage, std::move(actualOutputEmission),
         std::move(rtpSources), std::move(masterClock)});
    if (!bound) return PreparedResult::failure(bound.error());
    std::vector<MediaPreparedRealtimeInputBinding> inputs;
    for (const auto& target : bound.value().assembly.sourceTargets) {
        if (target.sourceIndex >= sealed.size() || target.sourceIndex >= plannedInputs.value().inputs.size())
            return PreparedResult::failure(::media::ErrorInfo::invalidArgument("Composition input binding source index is invalid"));
        auto input = std::move(sealed[target.sourceIndex]).release();
        if (!input) return PreparedResult::failure(input.error());
        inputs.push_back({target.primaryInput, MediaPreparedRealtimeInputKind::RawRtp, std::move(input.value().video)});
        const bool hasAudio = plannedInputs.value().inputs[target.sourceIndex].members == MediaTranscodeStreamSet::AudioVideo;
        if (target.isolatedAudioInput.has_value() != hasAudio || input.value().audio.has_value() != hasAudio)
            return PreparedResult::failure(::media::ErrorInfo::notInitialized("Composition input binding differs from planned members"));
        if (hasAudio) inputs.push_back({*target.isolatedAudioInput, MediaPreparedRealtimeInputKind::RawRtp, std::move(*input.value().audio)});
    }
    MediaRealtimeVideoPreparedReport report;
    report.inputType = RealtimeInputType::RtpPort;
    report.inputLayout = RealtimeInputStreamLayout::SeparateStreams;
    report.outputLayout = *request.output.streamLayout;
    report.outputTransport = *request.output.transport;
    report.streamSet = *request.parameters.execution.streamSet;
    report.selectedChain = "fixed-grid-composition";
    report.encoderName = encoder.ffmpegName;
    report.outputCodecName = encoder.codecName;
    report.hardwareDeviceKind = encoder.deviceKind();
    report.filterActive = true;
    for (std::size_t i = 0; i < prepared->sources.size(); ++i) {
        if (i != 0) { report.decoderName += ","; report.filterName += ","; }
        report.decoderName += prepared->sources[i].plan.decoder.ffmpegName;
        report.filterName += prepared->sources[i].plan.filter.filterName;
    }
    const auto& resolvedAudio = *audio.value().resolvedOutput;
    report.audio = MediaRealtimeVideoPreparedAudioReport{audio.value().branchMode, audio.value().reason,
        MediaRealtimeVideoPreparedAudioOutput{resolvedAudio.codecName(), resolvedAudio.sampleRate(), resolvedAudio.channels(),
            resolvedAudio.codecFrameSamples(), resolvedAudio.encoderName(), resolvedAudio.bitrateKbps()}};
    if (!request.output.sdpPath.empty()) report.outputDescription = {
        MediaRealtimeVideoOutputDescriptionKind::SessionDescriptionProtocol, request.output.sdpPath};
    MediaThreadingPolicy threading;
    threading.mode = MediaThreadingMode::PerNodeWorker;
    threading.priority = MediaThreadPriority::High;
    threading.collectWorkerMetrics = true;
    const auto preparationStorage = preparationBudget.value()->snapshot();
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::PlannerCapability,
        "composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=" +
        std::to_string(preparationStorage.capacity) + " retained=" +
        std::to_string(preparationStorage.retainedBytes) + " high_water=" +
        std::to_string(preparationStorage.highWaterBytes));
    return PreparedResult::success({{std::move(bound.value().graph), std::move(inputs),
        std::move(bound.value().assembly.runtimeBinding)}, threading, std::move(report)});
}
} // namespace media::ffmpeg::graph
