#include "internal/graph/planner/capability/MediaVideoSourceProducerAdapter.h"
#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
extern "C" {
#include <libavfilter/avfilter.h>
}
#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoFrameProducerFacts> MediaVideoSourceProducerAdapter::decoder(
    const MediaDecoderOutputAllocation& allocation, const AVFrame& frame)
{
    using Result = ::media::Result<MediaVideoFrameProducerFacts>;
    if (auto valid = MediaVideoFrameContractValidator::validate(frame, allocation.frame, "prepared decoder producer"); !valid)
        return Result::failure(valid.error());
    if (allocation.mechanism == MediaDecoderOutputAllocationMechanism::RkmppBorrowedSurface) {
        if (!MediaRkmppDependencyIdentity::matchesRuntime() ||
            allocation.frame.deviceKind != MediaHardwareDeviceKind::RKMPP ||
            (allocation.decoder.decoderName != "h264_rkmpp" && allocation.decoder.decoderName != "hevc_rkmpp"))
            return Result::failure(::media::ErrorInfo::unsupported("Decoder synchronous publication identity is not admitted"));
        MediaVideoFrameProducerFacts producer = MediaSynchronousFrameProducerFacts{
            MediaSynchronousFrameProducer::RkmppDecode, allocation.maximumExtendedBufferReferences};
        if (auto valid = validateOutput(frame, producer); !valid) return Result::failure(valid.error());
        return Result::success(std::move(producer));
    }
    if (!allocation.independentOfDecoderSurfaces())
        return Result::failure(::media::ErrorInfo::unsupported("Decoder publication mechanism is not admitted"));
    auto cuda = inspectMediaCudaFrameProducer(frame,
        allocation.mechanism == MediaDecoderOutputAllocationMechanism::NativeCudaSafeCopy
            ? MediaCudaFrameProducer::NativeSafeDecode : MediaCudaFrameProducer::CuvidDecode);
    if (!cuda) return Result::failure(cuda.error());
    cuda.value().maximumExtendedBufferReferences = allocation.maximumExtendedBufferReferences;
    return Result::success(MediaVideoFrameProducerFacts{std::move(cuda).value()});
}

::media::Result<MediaPreparedFilterProducerFacts> MediaVideoSourceProducerAdapter::filter(
    const AVFilterGraph& graph, std::string description, const AVFrame& input,
    const MediaVideoFrameProducerFacts& inputProducer)
{
    using Result = ::media::Result<MediaPreparedFilterProducerFacts>;
    const AVFilterContext* operation = nullptr;
    for (unsigned index = 0; index < graph.nb_filters; ++index) {
        const auto* node = graph.filters[index];
        if (!node || !node->filter || !node->filter->name)
            return Result::failure(::media::ErrorInfo::notInitialized("Prepared filter implementation is missing"));
        const std::string_view name(node->filter->name);
        if (name == "buffer" || name == "buffersink") continue;
        if (operation) return Result::failure(::media::ErrorInfo::unsupported("Prepared source requires one admitted spatial producer"));
        operation = node;
    }
    if (!operation) return Result::failure(::media::ErrorInfo::unsupported("Prepared source filter producer is absent"));
    const std::string_view name(operation->filter->name);
    if (const auto* cuda = std::get_if<MediaCudaFrameProducerFacts>(&inputProducer)) {
        if (name != "scale_cuda" && name != "null")
            return Result::failure(::media::ErrorInfo::unsupported("Prepared CUDA spatial producer is not admitted"));
        auto producer = inspectMediaCudaFrameProducer(input,
            name == "null" ? cuda->kind : MediaCudaFrameProducer::ScaleCuda);
        if (!producer) return Result::failure(producer.error());
        producer.value().maximumExtendedBufferReferences = cuda->maximumExtendedBufferReferences;
        return Result::success({MediaVideoFrameProducerFacts{std::move(producer).value()}, name == "null", std::nullopt});
    }
    const auto& synchronous = std::get<MediaSynchronousFrameProducerFacts>(inputProducer);
    if (synchronous.kind != MediaSynchronousFrameProducer::RkmppDecode || input.format != AV_PIX_FMT_DRM_PRIME)
        return Result::failure(::media::ErrorInfo::unsupported("Prepared synchronous source producer is not admitted"));
    auto isolation = MediaVideoSourceIsolationAdapter::inspect(graph, std::move(description));
    if (!isolation) return Result::failure(isolation.error());
    // Fixed hwcontext_rkmpp allocates its DRM descriptor in frame.buf, never
    // extended_buf. RGA creates this independent pool in init_hwframes_ctx.
    return Result::success({MediaSynchronousFrameProducerFacts{MediaSynchronousFrameProducer::RgaFilter, 0},
        false, std::move(isolation).value()});
}

::media::Status MediaVideoSourceProducerAdapter::validateOutput(
    const AVFrame& frame, const MediaVideoFrameProducerFacts& producer)
{
    if (const auto* cuda = std::get_if<MediaCudaFrameProducerFacts>(&producer)) {
        auto actual = inspectMediaCudaFrameProducer(frame, cuda->kind);
        if (!actual) return ::media::Status::failure(actual.error());
        if (actual.value().context != cuda->context || actual.value().stream != cuda->stream)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Prepared producer changed its authoritative CUDA stream"));
    } else if (frame.format != AV_PIX_FMT_DRM_PRIME || !frame.hw_frames_ctx || !frame.hw_frames_ctx->data) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Prepared synchronous producer lacks hardware frame ownership"));
    }
    return validateMediaVideoFrameReferenceContract(frame, producer);
}
}
