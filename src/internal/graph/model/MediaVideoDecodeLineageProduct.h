#pragma once
#include "media_transcode/Result.h"
#include <string_view>
#include <initializer_list>

namespace media::ffmpeg::graph {

// Internal planner products. SourceAccessUnit carries incomplete presentation
// timing through decoding; it is not valid canonical coverage or copy dispatch.
enum class MediaVideoDecodeLineageProduct {
    Canonical,
    SourceAccessUnit
};

constexpr std::string_view mediaVideoDecodeLineageProductName(
    MediaVideoDecodeLineageProduct product) noexcept
{
    switch (product) {
    case MediaVideoDecodeLineageProduct::Canonical: return "canonical";
    case MediaVideoDecodeLineageProduct::SourceAccessUnit: return "source_access_unit";
    }
    return {};
}

inline ::media::Result<MediaVideoDecodeLineageProduct> decodeMediaVideoDecodeLineageProduct(
    std::string_view value)
{
    for (const auto product : {MediaVideoDecodeLineageProduct::Canonical,
                              MediaVideoDecodeLineageProduct::SourceAccessUnit}) {
        if (value == mediaVideoDecodeLineageProductName(product))
            return ::media::Result<MediaVideoDecodeLineageProduct>::success(product);
    }
    return ::media::Result<MediaVideoDecodeLineageProduct>::failure(
        ::media::ErrorInfo::invalidArgument("Decoder lineage product is missing or invalid"));
}

} // namespace media::ffmpeg::graph
