#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaTsFinalGraphStoragePlanner.h"
#include "internal/graph/planner/capability/MediaVideoEncoderPreparer.h"
#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphResourceBinding.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedVideoCanvas.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionResourcePlanner.h"
#include "internal/graph/planner/realtime/MediaAvAggregateFrameCreditPlanner.h"

#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include <algorithm>
#include <limits>
#include <unordered_set>

namespace media::ffmpeg::graph {

::media::Result<MediaVideoCanvasRetentionPlan> MediaRealtimeCompositionTopology::planCanvasRetention() const
{
    if (!options_.outputVideo.encoder.preparedEmission)
        return ::media::Result<MediaVideoCanvasRetentionPlan>::failure(::media::ErrorInfo::notInitialized(
            "Composition canvas retention lacks prepared output encoder emission"));
    return MediaVideoCanvasRetentionPlanner::plan(graph_, output_.activationOwner,
        *options_.outputVideo.encoder.preparedEmission);
}

::media::Result<MediaVideoCanvasPreparationPlan> MediaRealtimeCompositionTopology::planCanvasPreparation(
    const MediaVideoCanvasAllocation& allocation) const
{
    auto retention = planCanvasRetention();
    if (!retention) return ::media::Result<MediaVideoCanvasPreparationPlan>::failure(retention.error());
    return MediaVideoCanvasPreparationPlanner::plan(options_.aggregate.canvas, retention.value(), allocation);
}

::media::Result<MediaRealtimeCompositionGraph> MediaRealtimeCompositionGraphBuilder::bind(
    MediaRealtimeCompositionTopology topology,
    const MediaRealtimeDeploymentLatencyBudget& latency,
    MediaRealtimeCompositionPreparedResources resources)
{
    using Result = ::media::Result<MediaRealtimeCompositionGraph>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    auto sourceResources = topology.planSourceResources();
    if (!sourceResources) return Result::failure(sourceResources.error());
    auto& graph = topology.graph_;
    auto& options = topology.options_;
    if (auto status = options.outputRuntime.scheduling.validate(
            options.outputRuntime.protocolOutput, latency.targetResidence); !status)
        return Result::failure(status.error());
    auto activation = options.outputRuntime.scheduling.activationLead(options.outputRuntime.protocolOutput);
    if (!activation || activation.value() != options.outputRuntime.activationOutputLead)
        return invalid("Composition output activation differs from encoded scheduling");
    if (graph.empty() || options.sources.empty() ||
        topology.sources_.size() != options.sources.size() ||
        resources.videoDecoders.size() != options.sources.size() ||
        resources.rtpSources.size() != options.sources.size() || !resources.masterClock ||
        !resources.videoEncoder || !resources.canvas)
        return invalid("Composition binding requires its topology and every prepared resource");
    for (std::size_t source = 0; source < options.sources.size(); ++source) {
        if (!resources.rtpSources[source])
            return invalid("Composition source lacks its continuous prepared RTP owner");
        const auto actual = resources.rtpSources[source]->preparationStorage();
        const auto& planned = options.sources[source].preparationStorage;
        const auto allocation = std::find_if(planned.begin(), planned.end(), [&](const auto& value) {
            return value.allocationOwner.get() == actual.allocationOwner.get() &&
                !value.allocationOwner.owner_before(actual.allocationOwner) &&
                !actual.allocationOwner.owner_before(value.allocationOwner);
        });
        const auto members = options.sources[source].runtime.synchronization.members;
        if (allocation == planned.end() || allocation->maximumAllocationBytes != actual.maximumAllocationBytes ||
            allocation->streamCount != actual.streamCount || allocation->authority != actual.authority ||
            allocation->sourceBudgets != actual.sourceBudgets ||
            actual.streamCount != (members == MediaTranscodeStreamSet::AudioVideo ? 2u : 1u))
            return invalid("Composition continuous RTP allocation differs from its admitted source budget");
        const bool decoded = std::holds_alternative<MediaDecodedVideoAssemblyTiming>(
            options.sources[source].runtime.assembly.video.timing);
        const auto& containers = options.sources[source].preparedPacketContainers;
        if (decoded != containers.has_value())
            return invalid("Composition prepared prefix differs from the source timing product");
        if (decoded) {
            const auto* sealed = resources.rtpSources[source]->sealedAdmission();
            const auto resource = resources.rtpSources[source]->videoReferenceResource();
            if (!sealed || !resource || sealed->owner != resource ||
                sealed->owner.owner_before(resource) || resource.owner_before(sealed->owner) ||
                !containers->video.prefixObjects || containers->video != sealed->video.bounds ||
                containers->video.prefixObjects != sealed->video.compressed.maximumUnits ||
                containers->audio.has_value() != sealed->audio.has_value() ||
                (containers->audio && (*containers->audio != sealed->audio->bounds ||
                    containers->audio->prefixObjects != sealed->audio->compressed.maximumUnits)))
                return invalid("Composition prefix capacity differs from the actual sealed source owner");
        }
    }
    auto* encoder = dynamic_cast<FFmpegCodecContextBuffer*>(resources.videoEncoder.get());
    if (!encoder || !encoder->context() || encoder->ownership() != FFmpegCodecContextOwnership::Owned)
        return invalid("Composition requires the owned actual prepared output codec context");
    if (const auto* ts = std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(&options.outputRuntime.protocolOutput)) {
        auto parameters = FFmpegCodecParametersMaterializer::fromContext(*encoder->context());
        if (!parameters) return Result::failure(parameters.error());
        auto config = MediaTsFfmpegStreamConfigMaterializer::video(ts->protocol.muxPlan(), *parameters.value());
        if (!config) return Result::failure(config.error());
    }
    auto evidence = MediaVideoEncoderPreparer::inspect(*encoder->context(), options.outputVideo.encoder);
    if (!evidence) return Result::failure(evidence.error());
    if (resources.outputEmission.video != evidence.value().emission || !resources.outputEmission.audio ||
        resources.outputEmission.audio->frameSizeSamples != options.outputRuntime.audioOutput.codecFrameSamples() ||
        resources.outputEmission.audio->accessUnitsPerSecondNumerator !=
            static_cast<std::uint64_t>(options.outputRuntime.audioOutput.sampleRate()) ||
        resources.outputEmission.audio->accessUnitsPerSecondDenominator !=
            static_cast<std::uint64_t>(options.outputRuntime.audioOutput.codecFrameSamples()))
        return invalid("Composition output allocation facts differ from prepared encoder readback");
    auto retention = topology.planCanvasRetention();
    if (!retention) return Result::failure(retention.error());
    if (resources.canvasStorage.surfaceCount != retention.value().writableSurfaces ||
        resources.canvasStorage.maximumHeaderCount != retention.value().maximumPublishedLeases)
        return invalid("Composition canvas storage differs from final topology retention");
    const MediaVideoCanvasPlan canvas{options.aggregate.canvas, resources.canvasStorage};
    if (auto status = resources.canvas->validateBinding(canvas, encoder->context()->hw_frames_ctx); !status)
        return Result::failure(status.error());
    std::unordered_set<const MediaPreparedVideoDecoder*> decoders;
    for (const auto& decoder : resources.videoDecoders) {
        if (!decoder || !decoders.insert(decoder.get()).second)
            return invalid("Composition sources require distinct prepared decoder owners");
        if (auto status = decoder->validateHardwareDevice(encoder->context()->hw_device_ctx); !status)
            return Result::failure(status.error());
    }
    if (!options.outputRuntime.encoderFifoRetention)
        return invalid("Composition output lacks its prepared audio FIFO retention");
    auto admission = MediaRealtimeCompositionResourcePlanner::plan(graph, topology.projection_,topology.identity_,sourceResources.value(),
        topology.output_.activationOwner, topology.output_.audioAggregate, options.aggregate, resources.canvasStorage,
        retention.value(), resources.outputEmission, *options.outputRuntime.encoderFifoRetention, latency);
    if (!admission) return Result::failure(admission.error());
    if (auto status = MediaTsFinalGraphStoragePlanner::apply(graph, {}); !status)
        return Result::failure(status.error());
    auto ledger = MediaFinalGraphResourceLedgerCompiler::compile(graph, admission.value());
    if (!ledger) return Result::failure(ledger.error());
    const auto& pool = ledger.value().encoderFramesPool;
    if (!pool || *pool != retention.value().encoderPool ||
        pool->initialPoolSurfaces == 0 || pool->authority.empty() ||
        pool->initialPoolSurfaces > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        return invalid("Composition final admission lacks its hardware pool contract");
    const auto* actualPool = reinterpret_cast<const AVHWFramesContext*>(encoder->context()->hw_frames_ctx->data);
    if (!actualPool || actualPool->initial_pool_size != static_cast<int>(pool->initialPoolSurfaces))
        return invalid("Composition prepared pool differs from its final topology contract");
    MediaNodeId codecResolver = MediaNodeId::invalid();
    for (const auto id : topology.output_.processingMembers) {
        const auto* node = graph.findNode(id);
        if (!node || node->kind != MediaNodeKind::CodecResolver) continue;
        if (codecResolver.isValid()) return invalid("Composition output has multiple video codec resolvers");
        codecResolver = id;
    }
    if (auto status = MediaRealtimeGraphResourceBinding::apply(graph, codecResolver, ledger.value()); !status)
        return Result::failure(status.error());
    auto aggregate = std::make_shared<const MediaAvContinuousAggregatePlan>(
        MediaAvContinuousAggregatePlan{std::move(options.aggregate), resources.canvasStorage});
    std::vector<MediaAvRuntimeDomainBinding> domains;
    for (std::size_t i = 0; i < options.sources.size(); ++i) {
        auto& runtime = options.sources[i].runtime;
        if (!resources.rtpSources[i])
            return invalid("Composition source lacks its continuous prepared RTP owner");
        if (auto valid = MediaOutputReservationStoragePlanner::validate(runtime.reservationStorage,
                runtime.synchronization.startup.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan
                    ? MediaLineageAllocationScope{MediaPreparedLineageAllocation{resources.rtpSources[i]->videoReferenceResource()}}
                    : MediaLineageAllocationScope{MediaRuntimeLineageAllocation{}}); !valid)
            return Result::failure(valid.error());
        domains.push_back({options.sources[i].fixedStorage, runtime.groupKey, std::move(runtime.synchronization),
            std::move(runtime.edgePolicies), MediaAvSourceDomainBinding{std::move(runtime.transition), std::move(topology.sources_[i]),
                nullptr, std::move(runtime.reservationStorage), std::move(resources.videoDecoders[i]), std::move(resources.rtpSources[i])}});
    }
    auto outputMembers = topology.output_.processingMembers;
    auto& output = options.outputRuntime;
    domains.push_back({nullptr, output.groupKey, std::move(output.synchronization),
        std::move(output.edgePolicies), MediaAvOutputDomainBinding{std::move(topology.output_), std::move(aggregate),
            std::move(resources.videoEncoder), std::move(resources.canvas)}});
    auto outputProduct = std::visit([]<typename Product>(Product&& product) -> MediaAvSyncRuntimeOutputProduct {
        return MediaAvSyncRuntimeOutputProduct(std::forward<Product>(product));
    }, std::move(output.protocolOutput));
    MediaAvSyncRuntimeBinding binding{std::move(domains), output.groupKey,
        std::move(output.datagramTransport), MediaSynchronizedAudioExecutionProduct::FrameTranscode,
        std::move(outputProduct), output.scheduling, std::move(resources.masterClock)};
    if (auto status = MediaAvSyncGraphShapeValidator::validate(graph, binding); !status)
        return Result::failure(status.error());
    return Result::success({std::move(graph),
        {std::move(binding), std::move(topology.targets_), std::move(outputMembers)}});
}

} // namespace media::ffmpeg::graph
