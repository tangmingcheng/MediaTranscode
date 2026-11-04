#include "internal/graph/sync/lineage/MediaVideoDecodeLineageOption.h"

namespace media::ffmpeg::graph {
::media::Result<std::optional<MediaVideoDecodeLineageProduct>>
parseMediaVideoDecodeLineageOption(const MediaNodeOptions* options, bool copyOpaque)
{
    using Result = ::media::Result<std::optional<MediaVideoDecodeLineageProduct>>;
    const bool capacity = options && options->has("video.lineage.capacity");
    const bool product = options && options->has("video.lineage.decoder_product");
    if (!capacity && !product) return Result::success(std::nullopt);
    if (!capacity || !product) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Decoder lineage requires both capacity and an explicit product"));
    auto decoded = decodeMediaVideoDecodeLineageProduct(options->value("video.lineage.decoder_product"));
    if (!decoded) return Result::failure(decoded.error());
    if (decoded.value() == MediaVideoDecodeLineageProduct::SourceAccessUnit && !copyOpaque)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source-AU decoding requires planned COPY_OPAQUE propagation"));
    return Result::success(decoded.value());
}
} // namespace media::ffmpeg::graph
