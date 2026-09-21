#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasProducer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}
#include <array>
#include <cstring>
#include <limits>
#include <new>

namespace media::ffmpeg::graph {
namespace {
struct HeaderLease final {
    std::shared_ptr<std::atomic_size_t> outstanding;
    std::shared_ptr<MediaNodeWakeup> availabilityWakeup;
};
void releaseHeader(void* opaque, std::uint8_t*)
{
    std::unique_ptr<HeaderLease> lease(static_cast<HeaderLease*>(opaque));
    --*lease->outstanding;
    // AVFrame releases its image buffers before extended_buf. The owner must
    // observe this release even when no source or channel can wake it anymore.
    lease->availabilityWakeup->notify();
}
::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}
}

::media::Status MediaVideoCanvasProducer::prepare(
    const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
    std::shared_ptr<MediaNodeWakeup> availabilityWakeup)
{
    if (black_) return invalid("canvas producer is already prepared");
    if (!availabilityWakeup) return invalid("canvas requires its owner availability wakeup");
    if (!productionHwFrames || !productionHwFrames->data || plan.width <= 0 || plan.height <= 0 ||
        !plan.surfaceCount || !plan.maximumHeaderCount || !plan.maximumSurfaceBytes ||
        !plan.maximumStagingBytes || plan.maximumStagingBytes > std::numeric_limits<int>::max() ||
        (plan.effectiveColorRange.range != AVCOL_RANGE_MPEG &&
        plan.effectiveColorRange.range != AVCOL_RANGE_JPEG))
        return invalid("canvas plan lacks explicit geometry, range or allocation bounds");
    const auto* pool = reinterpret_cast<const AVHWFramesContext*>(productionHwFrames->data);
    if (pool->format != plan.hardwareFormat || pool->sw_format != plan.softwareFormat ||
        pool->width != plan.width || pool->height != plan.height)
        return invalid("canvas plan does not match prepared encoder frames");
    const auto* descriptor = av_pix_fmt_desc_get(plan.softwareFormat);
    if (!descriptor) return invalid("canvas software format has no pixel descriptor");
    for (const auto& tile : plan.tiles) {
        if (tile.x < 0 || tile.y < 0 || tile.width <= 0 || tile.height <= 0 ||
            tile.width > plan.width || tile.height > plan.height ||
            tile.x > plan.width - tile.width || tile.y > plan.height - tile.height ||
            tile.x % (1 << descriptor->log2_chroma_w) || tile.y % (1 << descriptor->log2_chroma_h) ||
            tile.width % (1 << descriptor->log2_chroma_w) || tile.height % (1 << descriptor->log2_chroma_h))
            return invalid("canvas tile rectangle is outside the planned chroma grid");
    }
    auto created = MediaVideoCanvasAdapter::create(productionHwFrames);
    if (!created) return ::media::Status::failure(created.error());
    auto allocate = [&]() -> ::media::Result<FramePtr> {
        auto frame = makeFrame();
        if (!frame) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::allocationFailed("canvas frame header"));
        const int rc = av_hwframe_get_buffer(productionHwFrames, frame.get(), 0);
        if (rc < 0) return ::media::Result<FramePtr>::failure(FFmpegGraphError::fromCode(rc, "canvas production surface"));
        auto allocation = readMediaVideoCanvasAllocation(*frame);
        if (!allocation) return ::media::Result<FramePtr>::failure(allocation.error());
        if (allocation.value().surfaceBytes != plan.maximumSurfaceBytes ||
            allocation.value().stagingBytes != plan.maximumStagingBytes)
            return ::media::Result<FramePtr>::failure(::media::ErrorInfo::invalidArgument("canvas allocation differs from planner readback"));
        frame->color_range = plan.effectiveColorRange.range;
        return ::media::Result<FramePtr>::success(std::move(frame));
    };
    auto black = allocate();
    if (!black) return ::media::Status::failure(black.error());
    std::vector<FramePtr> surfaces;
    surfaces.reserve(plan.surfaceCount);
    for (std::size_t index = 0; index < plan.surfaceCount; ++index) {
        auto frame = allocate();
        if (!frame) return ::media::Status::failure(frame.error());
        surfaces.push_back(std::move(frame.value()));
    }
    auto staging = makeFrame();
    if (!staging) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("black staging header"));
    staging->format = plan.softwareFormat;
    staging->width = plan.width;
    staging->height = plan.height;
    staging->color_range = plan.effectiveColorRange.range;
    staging->buf[0] = av_buffer_alloc(static_cast<std::size_t>(plan.maximumStagingBytes));
    if (!staging->buf[0]) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("black staging pixels"));
    int rc = av_image_fill_arrays(staging->data, staging->linesize, staging->buf[0]->data,
        plan.softwareFormat, plan.width, plan.height, 1);
    if (rc < 0 || static_cast<std::uint64_t>(rc) != plan.maximumStagingBytes)
        return invalid("black staging allocation differs from planner readback");
    std::array<ptrdiff_t, 4> strides{};
    for (std::size_t i = 0; i < strides.size(); ++i) strides[i] = staging->linesize[i];
    rc = av_image_fill_black(staging->data, strides.data(), plan.softwareFormat,
        plan.effectiveColorRange.range, plan.width, plan.height);
    if (rc < 0) return FFmpegGraphError::statusFromCode(rc, "av_image_fill_black(canvas)");
    auto status = created.value()->upload(*staging, *black.value());
    if (!status) return status;
    auto readback = makeFrame();
    if (!readback) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("black readback header"));
    readback->format = staging->format;
    readback->width = staging->width;
    readback->height = staging->height;
    readback->buf[0] = av_buffer_alloc(static_cast<std::size_t>(plan.maximumStagingBytes));
    if (!readback->buf[0]) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("black readback pixels"));
    rc = av_image_fill_arrays(readback->data, readback->linesize, readback->buf[0]->data,
        plan.softwareFormat, plan.width, plan.height, 1);
    if (rc < 0) return FFmpegGraphError::statusFromCode(rc, "black readback layout");
    status = created.value()->download(*black.value(), *readback);
    if (!status) return status;
    if (std::memcmp(staging->buf[0]->data, readback->buf[0]->data,
        static_cast<std::size_t>(plan.maximumStagingBytes)) != 0)
        return invalid("hardware black template pixel readback differs from explicit color range");
    const MediaVideoCanvasRectangle full{0, 0, plan.width, plan.height};
    status = created.value()->validate(*black.value(), *surfaces.front(), full);
    if (!status) return status;
    frames_ = makeBufferRef(productionHwFrames);
    if (!frames_) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("canvas frames reference"));
    outstanding_ = std::make_shared<std::atomic_size_t>(0);
    availabilityWakeup_ = std::move(availabilityWakeup);
    plan_ = plan;
    adapter_ = std::move(created.value());
    surfaces_ = std::move(surfaces);
    black_ = std::move(black.value());
    return ::media::Status::success();
}

::media::Result<FramePtr> MediaVideoCanvasProducer::publish(const AVFrame& frame)
{
    if (outstanding_->load() >= plan_.maximumHeaderCount)
        return ::media::Result<FramePtr>::failure(::media::ErrorInfo::wouldBlock("canvas pending header bound reached"));
    FramePtr output(av_frame_clone(&frame));
    if (!output) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::allocationFailed("canvas output header"));
    auto* counter = new (std::nothrow) HeaderLease{outstanding_, availabilityWakeup_};
    if (!counter) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::allocationFailed("canvas header lease"));
    auto** refs = static_cast<AVBufferRef**>(av_realloc_array(output->extended_buf,
        output->nb_extended_buf + 1, sizeof(AVBufferRef*)));
    if (!refs) {
        delete counter;
        return ::media::Result<FramePtr>::failure(::media::ErrorInfo::allocationFailed("canvas lease references"));
    }
    output->extended_buf = refs;
    AVBufferRef* lease = av_buffer_create(nullptr, 0, releaseHeader, counter, 0);
    if (!lease) {
        delete counter;
        return ::media::Result<FramePtr>::failure(::media::ErrorInfo::allocationFailed("canvas header lease buffer"));
    }
    ++*outstanding_;
    output->extended_buf[output->nb_extended_buf++] = lease;
    return ::media::Result<FramePtr>::success(std::move(output));
}

::media::Result<FramePtr> MediaVideoCanvasProducer::cloneBlack()
{
    if (!black_) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::notInitialized("canvas is not prepared"));
    return publish(*black_);
}

::media::Result<FramePtr> MediaVideoCanvasProducer::compose(std::span<const AVFrame* const> tiles)
{
    if (!black_) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::notInitialized("canvas is not prepared"));
    if (tiles.size() != plan_.tiles.size())
        return ::media::Result<FramePtr>::failure(::media::ErrorInfo::invalidArgument("canvas tile count differs from plan"));
    bool any = false;
    for (const auto* tile : tiles) any |= tile != nullptr;
    if (!any) return cloneBlack();
    if (outstanding_->load() >= plan_.maximumHeaderCount)
        return ::media::Result<FramePtr>::failure(::media::ErrorInfo::wouldBlock("canvas pending header bound reached"));
    AVFrame* target = nullptr;
    for (auto& surface : surfaces_) if (av_frame_is_writable(surface.get())) { target = surface.get(); break; }
    if (!target) return ::media::Result<FramePtr>::failure(::media::ErrorInfo::wouldBlock("canvas surface pool exhausted"));
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (!tiles[index]) continue;
        auto status = adapter_->validate(*tiles[index], *target, plan_.tiles[index]);
        if (!status) return ::media::Result<FramePtr>::failure(status.error());
    }
    auto status = adapter_->copy(*black_, *target, {0, 0, plan_.width, plan_.height});
    if (!status) return ::media::Result<FramePtr>::failure(status.error());
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (!tiles[index]) continue;
        status = adapter_->copy(*tiles[index], *target, plan_.tiles[index]);
        if (!status) return ::media::Result<FramePtr>::failure(status.error());
    }
    return publish(*target);
}

} // namespace media::ffmpeg::graph
