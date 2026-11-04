#include "internal/graph/planner/realtime/MediaRtpSourceProbeGoalPlanner.h"
#include <string_view>
#include <new>
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "application/realtime/MediaRealtimeCompositionSourcePreparer.h"

#include "internal/graph/builder/codec/MediaVideoDecoderPlanOptionCodec.h"
#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

namespace media::ffmpeg::graph {
namespace {

struct PreparedTile final {
    ::media::ffmpeg::FramePtr frame;
    MediaVideoFrameReadiness readiness;
};
::media::Result<PreparedTile> prepareTile(
    MediaVideoSourcePlan& plan, const AVFrame& firstFrame,
    const MediaVideoFrameReadiness& inputReadiness,
    const std::shared_ptr<MediaPreparationMemoryResource>& resource,
    const MediaRealtimeRawInputPlan& raw, const MediaHardwareDescriptor& target,
    const MediaVideoSourceTiming& timing)
{
    using Result = ::media::Result<PreparedTile>;
    if (!plan.filterActive) {
        auto frame = ::media::ffmpeg::FramePtr(av_frame_clone(&firstFrame));
        if (!frame) return Result::failure(::media::ErrorInfo::allocationFailed(
            "Composition prepared tile reference"));
        if (auto valid = MediaVideoFrameContractValidator::validate(
                *frame, target, "composition prepared tile"); !valid)
            return Result::failure(valid.error());
        return Result::success({std::move(frame), inputReadiness});
    }
    if (!plan.filterExecution || !plan.filterExecution->valid() ||
        plan.filterExecution->output.filterDescription != plan.filter.filterName)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Prepared tile requires its planned filter execution contract"));
    MediaNodeOptions options;
    options.set("filter.pipeline.filter", plan.filter.filterName);
    std::optional<AVRational> frameRate;
    if (const auto* cadence = std::get_if<MediaObservedVideoCadence>(&timing))
        frameRate = AVRational{cadence->rate.num, cadence->rate.den};
    const AVRational timeBase = std::holds_alternative<MediaSourcePresentationTiming>(timing)
        ? firstFrame.time_base : AVRational{1, raw.videoTransport.clockRate};
    auto filter = VideoFilterGraphBuilder::build({&options, &firstFrame,
        timeBase, frameRate,
        {raw.video.sampleAspectRatio.num, raw.video.sampleAspectRatio.den}});
    if (!filter) return Result::failure(filter.error());
    bool scale = false;
    bool alias = false;
    for (unsigned i = 0; i < filter.value().graph->nb_filters; ++i) {
        const auto* node = filter.value().graph->filters[i];
        if (!node || !node->filter || !node->filter->name)
            return Result::failure(::media::ErrorInfo::notInitialized("Prepared filter implementation missing"));
        const std::string_view name(node->filter->name);
        if (name == "buffer" || name == "buffersink") continue;
        if (name == "scale_cuda" && !scale && !alias) scale = true;
        else if (name == "null" && !scale && !alias) alias = true;
        else return Result::failure(::media::ErrorInfo::unsupported("Prepared filter graph has no admitted producer stream contract"));
    }
    if ((!scale && !alias) || !plan.decoderReadiness)
        return Result::failure(::media::ErrorInfo::notInitialized("Prepared filter producer authority missing"));
    const auto* decoderProducer = std::get_if<MediaCudaFrameProducerFacts>(&*plan.decoderReadiness);
    if (!decoderProducer)
        return Result::failure(::media::ErrorInfo::unsupported("Prepared filter has no admitted synchronous producer adapter"));
    auto producer = inspectMediaCudaFrameProducer(firstFrame,
        scale ? MediaCudaFrameProducer::ScaleCuda : decoderProducer->kind);
    if (!producer) return Result::failure(producer.error());
    producer.value().maximumExtendedBufferReferences = decoderProducer->maximumExtendedBufferReferences;
    if (auto valid = validateMediaVideoFrameReferenceContract(firstFrame, *plan.decoderReadiness); !valid)
        return Result::failure(valid.error());
    if (auto waited = inputReadiness.enqueueCudaWait(producer.value().context, producer.value().stream); !waited)
        return Result::failure(waited.error());
    plan.filterExecution->inputConsumption = MediaVideoFilterInputConsumption::ImmediateSingleOutput;
    auto inputAllocations = MediaFfmpegPayloadReferences::capture(firstFrame, resource);
    if (!inputAllocations) return Result::failure(inputAllocations.error());
    auto tile = ::media::ffmpeg::makeFrame();
    if (!tile) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Composition prepared filtered tile"));
    MediaVideoProducerFailureScope failure(producer.value());
    // The prepared frame is not published to the runtime and has no production
    // lineage. Execute the same negotiated filter implementation used by nodes.
    const int sent = av_buffersrc_add_frame_flags(filter.value().bufferSource,
        const_cast<AVFrame*>(&firstFrame), AV_BUFFERSRC_FLAG_KEEP_REF);
    if (sent < 0) return Result::failure(FFmpegGraphError::fromCode(sent,
        "composition source prepared filter input"));
    const int received = av_buffersink_get_frame(filter.value().bufferSink, tile.get());
    if (received < 0) return Result::failure(FFmpegGraphError::fromCode(received,
        "composition source prepared filter output"));
    if (auto valid = MediaVideoFrameContractValidator::validate(
            *tile, target, "composition prepared filtered tile"); !valid)
        return Result::failure(valid.error());
    auto outputFacts = inspectMediaCudaFrameProducer(*tile, producer.value().kind);
    if (!outputFacts) return Result::failure(outputFacts.error());
    if (outputFacts.value().context != producer.value().context || outputFacts.value().stream != producer.value().stream)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared filter changed its authoritative CUDA stream"));
    outputFacts.value().maximumExtendedBufferReferences = producer.value().maximumExtendedBufferReferences;
    plan.filterReadiness = outputFacts.value();
    if (auto valid = validateMediaVideoFrameReferenceContract(*tile, *plan.filterReadiness); !valid)
        return Result::failure(valid.error());
    if (outputFacts.value().maximumExtendedBufferReferences) {
        auto storage = MediaVideoFrameOwnershipStoragePlanner::plan({
            *outputFacts.value().maximumExtendedBufferReferences, true, MediaVideoFrameReadiness::Kind::CudaEvent});
        if (!storage) return Result::failure(storage.error());
        plan.filterOwnershipStorage = storage.value();
    }
    if (alias) {
        failure.release();
        return Result::success({std::move(tile), inputReadiness});
    }
    auto readiness = captureMediaVideoFrameReadiness(*tile, resource, *plan.filterReadiness, inputAllocations.value());
    if (!readiness) return Result::failure(readiness.error());
    failure.release();
    return Result::success({std::move(tile), std::move(readiness).value()});
}

} // namespace

::media::Result<MediaRealtimeCompositionPreparedSourceVideo>
MediaRealtimeCompositionSourcePreparer::prepare(
    MediaRealtimeCompositionCapturedSource& captured,
    const MediaRealtimeCompositionSourceFacts& facts, MediaVideoSourcePlan candidate,
    const MediaHardwareDescriptor& target, AVBufferRef* device,
    std::stop_token cancellation, const MediaSharedPreparationStorage& storageScope) try
{
    using Result = ::media::Result<MediaRealtimeCompositionPreparedSourceVideo>;
    const auto& raw = facts.raw;
    if (!captured.masterClock || !storageScope.root || !captured.session) return Result::failure(::media::ErrorInfo::notInitialized(
        "Composition source has no installed protocol session, preparation clock or storage owner"));
    const MediaPreparationControl control{captured.deadline, cancellation};
    if (auto status = captured.capture.validateContinuousPreparation(captured.session); !status)
        return Result::failure(status.error());
    if (auto status = control.check("composition decoder preparation"); !status)
        return Result::failure(status.error());
    auto resources = captured.capture.resources();
    if (!resources) return Result::failure(resources.error());
    auto descriptor = MediaRawRtpStreamDescriptorFactory::create(raw.videoDepacketizer);
    if (!descriptor) return Result::failure(descriptor.error());
    const auto* snapshot = descriptor.value()->inputStreamSnapshot(raw.video.streamIndex);
    if (!snapshot || !candidate.decoder.outputFrame)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition decoder requires its prepared RTP stream descriptor"));
    auto parameters = snapshot->cloneCodecParameters();
    if (!parameters) return Result::failure(parameters.error());
    auto options = MediaVideoDecoderPlanOptionCodec::encode(candidate.decoder, std::nullopt);
    if (!options) return Result::failure(options.error());
    auto decoded = CodecResolverDecoderContextBuilder::build({
        parameters.value().get(), snapshot->time, &options.value(), device, candidate.decoderLineagePropagation});
    if (!decoded) return Result::failure(decoded.error());
    auto decoder = std::move(decoded).value();
    if (auto status = captured.capture.validateContinuousPreparation(captured.session); !status)
        return Result::failure(status.error());
    if (auto status = control.check("composition decoder open"); !status)
        return Result::failure(status.error());
    auto retention = MediaDecoderInputRetentionAdapter::readAfterOpen(
        *decoder.context, candidate.decoder.hwaccelName);
    if (!retention) return Result::failure(::media::ErrorInfo::unsupported(
        "Composition prepared decoder has no authoritative packet retention"));
    candidate.decoder.preparedInputRetention = *retention;
    if (captured.sessionVideoPlan) {
        const auto* original = &*captured.sessionVideoPlan;
        if (original->decoder.ffmpegName != candidate.decoder.ffmpegName ||
            original->decoder.hwaccelName != candidate.decoder.hwaccelName ||
            original->decoder.preparedInputRetention != candidate.decoder.preparedInputRetention ||
            original->decoder.outputFrame != candidate.decoder.outputFrame ||
            original->decoderLineagePropagation != candidate.decoderLineagePropagation ||
            original->filterActive != candidate.filterActive ||
            original->filter.filterName != candidate.filter.filterName ||
            original->filter.inputFrame != candidate.filter.inputFrame ||
            original->filter.outputFrame != candidate.filter.outputFrame ||
            original->transferDirection != candidate.transferDirection)
            return Result::failure(::media::ErrorInfo::unsupported(
                "Source candidate differs from the installed continuous admission facts"));
    } else {
        // Protocol installation is independent of decoder selection. Publish
        // the first authoritative decoder contract only after its full copy is
        // ready; a later probe/filter failure never rolls this commitment back.
        auto sessionVideoPlan = candidate;
        captured.sessionVideoPlan.emplace(std::move(sessionVideoPlan));
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::PlannerCapability,
            "composition_decoder_contract_bound source=" + facts.request.mediaId +
            " decoder=" + candidate.decoder.ffmpegName);
    }
    auto maximumDatagram = resources.value().video.rawRtpMaximumDatagramBytes();
    if (!maximumDatagram) return Result::failure(maximumDatagram.error());
    const MediaRtpReorderConfig reorder{raw.videoTransport.reorderWindowPackets,
        std::chrono::milliseconds(raw.videoTransport.maximumReorderDelayMs),
        raw.videoTransport.payloadType};
    auto storage = MediaSourcePreparationStoragePlanner::plan(maximumDatagram.value(),
        reorder, raw.videoDepacketizer, raw.videoAccessUnitEnvelope, *retention,
        decoder, *candidate.decoder.outputFrame, MediaSourcePreparationInput::CompletedAccessUnits);
    if (!storage) return Result::failure(storage.error());
    auto budget = MediaPreparationStorageBudget::create(storage.value().maximumBytes, storageScope, MediaStorageReleaseUnobserved{});
    if (!budget) return Result::failure(budget.error());
    auto reserved = budget.value()->reserve(storage.value().maximumBytes, MediaStorageAdmission::RequiredAllocation);
    if (!reserved) return Result::failure(reserved.error());
    std::optional<MediaRtpSourceDecoderProbeOwner> owner;
    owner.emplace(std::move(reserved).value(), std::move(decoder));
    for (;;) {
        std::uint64_t frontier = 0;
        std::optional<MediaRtpSourceFirstFrameEvidence> evidence;
        {
            auto snapshot = captured.session->probeSnapshot();
            if (!snapshot) return Result::failure(snapshot.error());
            frontier = snapshot.value().revision;
            if (snapshot.value().packets.empty()) {
                if (auto waited = captured.session->waitForAccessUnits(frontier, captured.deadline, cancellation); !waited)
                    return Result::failure(waited.error());
                continue;
            }
            auto goal = MediaRtpSourceProbeGoalPlanner::plan(facts.timing,
                facts.request.mediaId, snapshot.value().packets, storageScope.root);
            if (!goal) return Result::failure(goal.error());
            const auto policy = std::holds_alternative<MediaH264SignalingFacts>(
                captured.observation.signaling.facts)
                ? MediaRtpVideoPacketizationPolicy::H264NonInterleaved
                : MediaRtpVideoPacketizationPolicy::HevcNonInterleavedNoDonl;
            auto probed = MediaRtpSourceFirstFrameProbe::probe(snapshot.value().packets, {
                std::move(goal).value(), captured.observation.signaling, *parameters.value(), candidate,
                policy, raw.videoDepacketizer, reorder, raw.videoAccessUnitEnvelope,
                maximumDatagram.value(), {1, raw.videoTransport.clockRate},
                raw.video.sampleAspectRatio, captured.deadline, cancellation,
                MediaSourcePreparationInput::CompletedAccessUnits},
                std::move(*owner));
            if (!probed) return Result::failure(probed.error());
            evidence.emplace(std::move(probed).value());
        }
        if (evidence->disposition == MediaRtpSourceFirstFrameDisposition::FirstFrame) {
            auto allocation = MediaDecoderOutputAllocationAdapter::inspect(evidence->decoder.runtimeFacts,
                evidence->firstFrame->hw_frames_ctx, *candidate.decoder.outputFrame);
            if (!allocation) return Result::failure(allocation.error());
            candidate.decoderOutputAllocation = std::move(allocation).value();
            if (!candidate.decoderOutputAllocation || candidate.transferDirection != MediaHardwareTransferDirection::None)
                return Result::failure(::media::ErrorInfo::unsupported("Composition source lacks authoritative decoder readiness and direct transfer facts"));
            if (candidate.decoderOutputAllocation->mechanism != MediaDecoderOutputAllocationMechanism::NativeCudaSafeCopy &&
                candidate.decoderOutputAllocation->mechanism != MediaDecoderOutputAllocationMechanism::CuvidCopy)
                return Result::failure(::media::ErrorInfo::unsupported("Prepared decoder allocation authority unsupported"));
            const auto authority = candidate.decoderOutputAllocation->mechanism == MediaDecoderOutputAllocationMechanism::NativeCudaSafeCopy
                ? MediaCudaFrameProducer::NativeSafeDecode : MediaCudaFrameProducer::CuvidDecode;
            auto producer = inspectMediaCudaFrameProducer(*evidence->firstFrame, authority);
            if (!producer) return Result::failure(producer.error());
            if (std::holds_alternative<MediaSourcePresentationTiming>(facts.timing)) {
                // Both admitted decoder mechanisms publish copied CUDA video
                // data buffers; no engine canvas lease exists on this source.
                producer.value().maximumExtendedBufferReferences =
                    candidate.decoderOutputAllocation->maximumExtendedBufferReferences;
                auto storage = MediaVideoFrameOwnershipStoragePlanner::plan({
                    *producer.value().maximumExtendedBufferReferences, true,
                    MediaVideoFrameReadiness::Kind::CudaEvent});
                if (!storage) return Result::failure(storage.error());
                candidate.decoderOwnershipStorage = storage.value();
            }
            candidate.decoderReadiness = producer.value();
            auto readiness = captureMediaVideoFrameReadiness(*evidence->firstFrame,
                captured.session->videoReferenceResource(), *candidate.decoderReadiness, nullptr);
            if (!readiness) return Result::failure(readiness.error());
            auto tile = prepareTile(candidate, *evidence->firstFrame, readiness.value(),
                captured.session->videoReferenceResource(), raw, target, facts.timing);
            if (!tile) return Result::failure(tile.error());
            return Result::success({std::move(candidate), std::move(descriptor).value(), std::move(*evidence),
                std::move(tile.value().frame), std::move(tile.value().readiness)});
        }
        if (auto restarted = MediaRtpSourceFirstFrameProbe::restartDecoder(
                *evidence, *snapshot, candidate, {captured.deadline, cancellation}); !restarted)
            return Result::failure(restarted.error());
        owner.reset();
        owner.emplace(std::move(evidence->storage), std::move(evidence->decoder));
        evidence.reset();
        if (auto waited = captured.session->waitForAccessUnits(frontier, captured.deadline, cancellation); !waited)
            return Result::failure(waited.error());
    }
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRealtimeCompositionPreparedSourceVideo>::failure(
        ::media::ErrorInfo::allocationFailed("Composition source candidate preparation allocation failed"));
}

} // namespace media::ffmpeg::graph
