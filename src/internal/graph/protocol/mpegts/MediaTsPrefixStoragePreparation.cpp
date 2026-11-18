#include "internal/graph/protocol/mpegts/MediaTsPrefixStoragePreparation.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaTsPrefixStoragePreparation>
MediaTsPrefixStoragePreparation::prepare(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    const MediaTsProtocolSeriesStorageGeometry& geometry,
    bool needsCommitModel)
{
    using Result = ::media::Result<MediaTsPrefixStoragePreparation>;
    if (!resource)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix preparation requires its generation memory resource"));
    const auto footprint = [](const MediaSharedAllocationGeometry& shape) {
        if (!shape.count || !shape.bytes || shape.bytes % shape.count)
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "TS prefix allocation geometry is incomplete"));
        return MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
    };
    auto viewCost = footprint(geometry.batchOwner);
    if (!viewCost) return Result::failure(viewCost.error());
    std::size_t modelCost = 0;
    if (needsCommitModel) {
        auto cost = footprint(geometry.commitModel);
        if (!cost) return Result::failure(cost.error());
        modelCost = cost.value();
    }
    if (modelCost > (std::numeric_limits<std::size_t>::max)() - viewCost.value())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix preparation footprint overflows"));

    // Admit the complete publication before changing any cursor, continuity,
    // or schedule state. All partial physical allocations unwind by RAII.
    auto admitted = resource->reserveStorage(
        viewCost.value() + modelCost, MediaStorageAdmission::Backpressure);
    if (!admitted) return Result::failure(admitted.error());
    auto viewLease = admitted.value().split(viewCost.value());
    if (!viewLease) return Result::failure(viewLease.error());
    auto view = resource->allocateFromReservation(
        geometry.batchOwner.bytes, geometry.batchOwner.alignment,
        std::move(viewLease).value());
    if (!view) return Result::failure(view.error());
    MediaTsPrefixStoragePreparation output{std::move(view).value(), std::nullopt};
    if (needsCommitModel) {
        auto model = resource->allocateFromReservation(
            geometry.commitModel.bytes, geometry.commitModel.alignment,
            std::move(admitted).value());
        if (!model) return Result::failure(model.error());
        output.commitModel.emplace(std::move(model).value());
    }
    return Result::success(std::move(output));
}

} // namespace media::ffmpeg::graph
