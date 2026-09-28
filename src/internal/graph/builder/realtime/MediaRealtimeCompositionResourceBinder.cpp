#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphResourceBinding.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedVideoCanvas.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"

#include <limits>
#include <unordered_set>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeCompositionGraph> MediaRealtimeCompositionGraphBuilder::bind(
    MediaRealtimeCompositionTopology topology,
    const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
    MediaRealtimeCompositionPreparedResources resources)
{
    using Result = ::media::Result<MediaRealtimeCompositionGraph>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    auto& graph = topology.graph_;
    auto& options = topology.options_;
    if (graph.empty() || options.sources.empty() ||
        topology.sources_.size() != options.sources.size() ||
        resources.videoDecoders.size() != options.sources.size() ||
        !resources.videoEncoder || !resources.canvas)
        return invalid("Composition binding requires its topology and every prepared resource");
    const auto* encoder = dynamic_cast<const FFmpegCodecContextBuffer*>(resources.videoEncoder.get());
    if (!encoder || !encoder->context() || encoder->ownership() != FFmpegCodecContextOwnership::Owned)
        return invalid("Composition requires the owned actual prepared output codec context");
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
    // Recompile from this exact final graph. A caller cannot label a foreign or
    // partial ledger admitted. Unsupported aggregate accounting remains failure.
    auto ledger = MediaFinalGraphResourceLedgerCompiler::compile(graph, planningLedger, {});
    if (!ledger) return Result::failure(ledger.error());
    const auto& pool = ledger.value().encoderFramesPool;
    if (!pool || pool->initialPoolSurfaces == 0 || pool->authority.empty() ||
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
        domains.push_back({runtime.groupKey, std::move(runtime.synchronization),
            MediaAvSourceDomainBinding{std::move(runtime.transition), std::move(topology.sources_[i]),
                nullptr, std::move(resources.videoDecoders[i])}});
    }
    auto outputMembers = topology.output_.processingMembers;
    auto& output = options.outputRuntime;
    domains.push_back({output.groupKey, std::move(output.synchronization),
        MediaAvOutputDomainBinding{std::move(topology.output_), std::move(aggregate),
            std::move(resources.videoEncoder), std::move(resources.canvas)}});
    auto outputProduct = std::visit([]<typename Product>(Product&& product) -> MediaAvSyncRuntimeOutputProduct {
        return MediaAvSyncRuntimeOutputProduct(std::forward<Product>(product));
    }, std::move(output.protocolOutput));
    MediaAvSyncRuntimeBinding binding{std::move(domains), output.groupKey, std::move(output.edgePolicies),
        std::move(output.datagramTransport), MediaSynchronizedAudioExecutionProduct::FrameTranscode,
        std::move(outputProduct)};
    if (auto status = MediaAvSyncGraphShapeValidator::validate(graph, binding); !status)
        return Result::failure(status.error());
    return Result::success({std::move(graph),
        {std::move(binding), std::move(topology.targets_), std::move(outputMembers)}});
}

} // namespace media::ffmpeg::graph
