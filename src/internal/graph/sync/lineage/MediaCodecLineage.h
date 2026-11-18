#pragma once

#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaVideoDecodeLineage.h"
#include "internal/graph/sync/lineage/MediaVideoDisplayLineage.h"
#include "internal/graph/sync/lineage/MediaVideoEncodeLineage.h"
#include <concepts>
#include <type_traits>
#include <variant>

namespace media::ffmpeg::graph {

using MediaCodecLineage = std::variant<
    std::shared_ptr<const MediaCanonicalLineage>,
    std::shared_ptr<const MediaVideoDecodeLineage>,
    std::shared_ptr<const MediaVideoDisplayLineage>,
    MediaVideoEncodeLineage>;

template<class T>
concept MediaCodecLineageProduct = std::same_as<T, MediaCanonicalLineage> ||
    std::same_as<T, MediaVideoDecodeLineage> ||
    std::same_as<T, MediaVideoDisplayLineage> ||
    std::same_as<T, MediaVideoEncodeLineage>;

template<MediaCodecLineageProduct T>
using MediaCodecLineageValue = std::conditional_t<
    std::same_as<T, MediaVideoEncodeLineage>, T, std::shared_ptr<const T>>;

::media::Status validateMediaCodecLineage(const MediaCodecLineage& lineage);
std::uint64_t mediaCodecLineageGeneration(const MediaCodecLineage& lineage) noexcept;

template<MediaCodecLineageProduct T>
::media::Result<MediaCodecLineageValue<T>> selectMediaCodecLineage(
    const MediaCodecLineage& lineage)
{
    const auto* selected = std::get_if<MediaCodecLineageValue<T>>(&lineage);
    if (!selected) {
        return ::media::Result<MediaCodecLineageValue<T>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage does not match the consumer's planned product"));
    }
    const bool present = [&] {
        if constexpr (std::same_as<T, MediaVideoEncodeLineage>)
            return static_cast<bool>(selected->canonical);
        else
            return static_cast<bool>(*selected);
    }();
    if (!present)
        return ::media::Result<MediaCodecLineageValue<T>>::failure(
            ::media::ErrorInfo::invalidArgument("Codec lineage product is empty"));
    return ::media::Result<MediaCodecLineageValue<T>>::success(*selected);
}

} // namespace media::ffmpeg::graph
