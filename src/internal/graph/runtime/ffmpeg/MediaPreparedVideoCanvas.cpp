#include "internal/graph/runtime/ffmpeg/MediaPreparedVideoCanvas.h"
#include <new>
#include <stdexcept>

namespace media::ffmpeg::graph {
namespace {
bool samePlan(const MediaVideoCanvasPlan& left, const MediaVideoCanvasPlan& right)
{
    if (left.geometry.hardwareFormat != right.geometry.hardwareFormat || left.geometry.softwareFormat != right.geometry.softwareFormat ||
        left.geometry.width != right.geometry.width || left.geometry.height != right.geometry.height ||
        left.geometry.effectiveColorRange != right.geometry.effectiveColorRange || left.storage.surfaceCount != right.storage.surfaceCount ||
        left.storage.maximumSurfaceBytes != right.storage.maximumSurfaceBytes ||
        left.storage.maximumStagingBytes != right.storage.maximumStagingBytes ||
        left.storage.maximumHeaderCount != right.storage.maximumHeaderCount || left.geometry.tiles.size() != right.geometry.tiles.size())
        return false;
    for (std::size_t i = 0; i < left.geometry.tiles.size(); ++i) {
        const auto& a = left.geometry.tiles[i];
        const auto& b = right.geometry.tiles[i];
        if (a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height) return false;
    }
    return true;
}
}

::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>> MediaPreparedVideoCanvas::prepare(
    const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
    std::span<const AVFrame* const> preparedTiles)
try {
    using Result = ::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>>;
    if (preparedTiles.empty() || preparedTiles.size() != plan.geometry.tiles.size())
        return Result::failure(::media::ErrorInfo::invalidArgument("prepared canvas requires every real tile frame"));
    for (const auto* tile : preparedTiles) if (!tile)
        return Result::failure(::media::ErrorInfo::invalidArgument("prepared canvas tile frame is missing"));
    auto prepared = std::shared_ptr<MediaPreparedVideoCanvas>(new MediaPreparedVideoCanvas);
    auto producer = std::make_unique<MediaVideoCanvasProducer>();
    auto status = producer->prepare(plan, productionHwFrames);
    if (!status) return Result::failure(status.error());
    // Exercise the actual platform operation with production tile frames, not
    // only matching format names or allocating a same-shaped surrogate pool.
    for (std::size_t i = 0; i < preparedTiles.size(); ++i) {
        status = producer->adapter_->validate(*preparedTiles[i], *producer->surfaces_.front(), plan.geometry.tiles[i]);
        if (!status) return Result::failure(status.error());
        status = producer->adapter_->copy(*preparedTiles[i], *producer->surfaces_.front(), plan.geometry.tiles[i]);
        if (!status) return Result::failure(status.error());
    }
    prepared->producer_ = std::move(producer);
    return Result::success(std::move(prepared));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>>::failure(
        ::media::ErrorInfo::allocationFailed("prepared canvas resources"));
} catch (const std::length_error&) {
    return ::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>>::failure(
        ::media::ErrorInfo::invalidArgument("prepared canvas allocation bound exceeds container limits"));
}

::media::Status MediaPreparedVideoCanvas::validateBindingLocked(
    const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const
{
    if (!producer_) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("prepared canvas was already claimed"));
    if (!productionHwFrames || productionHwFrames->data != producer_->frames_->data ||
        !samePlan(plan, producer_->plan_))
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("prepared canvas production contract differs"));
    return ::media::Status::success();
}

::media::Status MediaPreparedVideoCanvas::validateBinding(
    const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const
{
    std::lock_guard lock(mutex_);
    return validateBindingLocked(plan, productionHwFrames);
}

::media::Result<std::unique_ptr<MediaVideoCanvasProducer>> MediaPreparedVideoCanvas::claim(
    const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
    std::shared_ptr<MediaNodeWakeup> availabilityWakeup)
{
    using Result = ::media::Result<std::unique_ptr<MediaVideoCanvasProducer>>;
    std::lock_guard lock(mutex_);
    auto status = validateBindingLocked(plan, productionHwFrames);
    if (!status) return Result::failure(status.error());
    if (!availabilityWakeup) return Result::failure(
        ::media::ErrorInfo::invalidArgument("prepared canvas requires its owner availability wakeup"));
    producer_->availabilityWakeup_ = std::move(availabilityWakeup);
    return Result::success(std::move(producer_));
}

} // namespace media::ffmpeg::graph
