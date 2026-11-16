#include "internal/graph/sync/lineage/MediaCodecLineage.h"
#include "internal/graph/sync/lineage/MediaDecodedVideoPresentation.h"
#include <type_traits>

namespace media::ffmpeg::graph {

::media::Status validateMediaCodecLineage(const MediaCodecLineage& lineage)
{
    return std::visit([](const auto& value) -> ::media::Status {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, MediaVideoEncodeLineage>) {
            if (!value.canonical)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Video encode lineage requires canonical identity"));
            if (auto status = validateMediaCanonicalLineage(*value.canonical); !status)
                return status;
            if (value.presentationProgress &&
                (!value.completionOrder || *value.completionOrder == 0 || !value.encoderInputPts ||
                 value.presentationProgress->followingFramesLowerBound < value.canonical->presentation))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Video encode producer progress precedes its carrying frame"));
            return ::media::Status::success();
        } else {
            if (!value) return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Codec lineage registry rejects null lineage"));
            using Product = typename std::decay_t<decltype(value)>::element_type;
            if constexpr (std::is_same_v<Product, const MediaCanonicalLineage>) {
                return validateMediaCanonicalLineage(*value);
            } else if constexpr (std::is_same_v<Product, const MediaVideoDisplayLineage>) {
                if (!value->accessUnit || value->decodedSequence == 0)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Video display lineage requires AU identity and decoded sequence"));
                if (auto status = validateMediaCodecLineage(MediaCodecLineage{value->accessUnit}); !status)
                    return status;
                auto projected = projectMediaDecodedVideoPresentation(*value->accessUnit,
                    value->decodedPresentation, value->decodedTimeBase);
                if (!projected) return ::media::Status::failure(projected.error());
                if (projected.value() != value->presentation)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Video display projection differs from its captured presentation"));
                return ::media::Status::success();
            } else {
                if (value->identity.sourceIdentity.empty() ||
                    value->identity.sourceSequence.value() == 0 || value->generation == 0 ||
                    value->packetPresentation == invalidMediaTimeValue ||
                    value->packetTimeBase.num <= 0 || value->packetTimeBase.den <= 0 ||
                    (value->duration && value->duration->nanoseconds() <= 0) ||
                    (value->mappingConfidence != MediaTimeMappingConfidence::Locked &&
                     value->mappingConfidence != MediaTimeMappingConfidence::Degraded)) {
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Video decode lineage requires source identity and valid timing facts"));
                }
                auto projected = value->projection.project(value->packetPresentation);
                if (!projected) return ::media::Status::failure(projected.error());
                if (projected.value() != value->presentation)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Video AU projection differs from its captured presentation"));
                return ::media::Status::success();
            }
            }
    }, lineage);
}

std::uint64_t mediaCodecLineageGeneration(const MediaCodecLineage& lineage) noexcept
{
    return std::visit([](const auto& value) -> std::uint64_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, MediaVideoEncodeLineage>) {
            return value.canonical ? value.canonical->generation : 0;
        } else {
        using Product = typename std::decay_t<decltype(value)>::element_type;
        if constexpr (std::is_same_v<Product, const MediaVideoDisplayLineage>)
            return value && value->accessUnit ? value->accessUnit->generation : 0;
        else
            return value ? value->generation : 0;
        }
    }, lineage);
}

} // namespace media::ffmpeg::graph
