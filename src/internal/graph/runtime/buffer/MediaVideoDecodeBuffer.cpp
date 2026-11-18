#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaCodecLineage.h"
#include "internal/graph/sync/lineage/MediaDecodedVideoPresentation.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include <new>

namespace media::ffmpeg::graph {

::media::Result<MediaVideoDecodeAllocationGeometry> MediaVideoDecodeBuffer::allocationGeometry()
{
    using R = ::media::Result<MediaVideoDecodeAllocationGeometry>;
    // Match PacketClock's const AU and the actual PMR allocators at creation.
    auto accessUnit = probeMediaSharedAllocationGeometry<const MediaVideoDecodeLineage>(
        std::type_identity<std::pmr::polymorphic_allocator<MediaVideoDecodeLineage>>{});
    auto display = probeMediaSharedAllocationGeometry<MediaVideoDisplayLineage>(
        std::type_identity<std::pmr::polymorphic_allocator<MediaVideoDisplayLineage>>{});
    auto wrapper = probeMediaSharedAllocationGeometry<MediaVideoDecodeBuffer>(
        std::type_identity<std::pmr::polymorphic_allocator<MediaVideoDecodeBuffer>>{});
    if (!accessUnit) return R::failure(accessUnit.error());
    if (!display) return R::failure(display.error());
    if (!wrapper) return R::failure(wrapper.error());
    return R::success({accessUnit.value(), display.value(), wrapper.value()});
}

MediaVideoDecodeBuffer::MediaVideoDecodeBuffer(ConstructionKey, MediaBufferRef media,
    std::shared_ptr<const MediaVideoDecodeLineage> lineage,
    std::shared_ptr<const MediaVideoDisplayLineage> display)
    : m_media(std::move(media)), m_lineage(std::move(lineage)),
      m_display(std::move(display))
{
    setStreamKind(MediaStreamKind::Video);
    setPayloadKind(m_media->payloadKind());
    setFormatDescriptor(m_media->formatDescriptor());
    setTimeDescriptor(m_media->timeDescriptor());
    setHardwareDescriptor(m_media->hardwareDescriptor());
    setTimestamps(m_media->pts(), m_media->dts(), m_media->duration());
    setFlags(m_media->flags());
}

::media::Result<MediaBufferRef> MediaVideoDecodeBuffer::create(
    std::pmr::memory_resource& allocation, MediaBufferRef media, std::shared_ptr<const MediaVideoDecodeLineage> lineage,
    std::optional<std::uint64_t> decodedSequence)
try
{
    using Result = ::media::Result<MediaBufferRef>;
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(media.get());
    const auto* frame = dynamic_cast<const FFmpegFrameBuffer*>(media.get());
    if (!media || media->streamKind() != MediaStreamKind::Video ||
        (!packet && !frame) || (packet && !packet->packet()) || (frame && !frame->frame()) ||
        static_cast<bool>(frame) != decodedSequence.has_value() ||
        (decodedSequence && *decodedSequence == 0)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video decode buffer requires raw video media and matching display identity"));
    }
    if (auto valid = validateMediaCodecLineage(MediaCodecLineage{lineage}); !valid)
        return Result::failure(valid.error());
    const auto time = media->timeDescriptor().timeBase;
    if (time.num <= 0 || time.den <= 0 ||
        (packet && (packet->packet()->pts != lineage->packetPresentation ||
            time.num != lineage->packetTimeBase.num || time.den != lineage->packetTimeBase.den)) ||
        (frame && frame->frame()->pts == AV_NOPTS_VALUE)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video decode buffer requires authoritative packet or decoded frame timing"));
    }
    std::shared_ptr<const MediaVideoDisplayLineage> display;
    if (frame) {
        auto projected = projectMediaDecodedVideoPresentation(*lineage, frame->frame()->pts, time);
        if (!projected) return Result::failure(projected.error());
        display = std::allocate_shared<MediaVideoDisplayLineage>(
            std::pmr::polymorphic_allocator<MediaVideoDisplayLineage>{&allocation},
            MediaVideoDisplayLineage{lineage, *decodedSequence, frame->frame()->pts,
                time, projected.value()});
    }
    return Result::success(std::allocate_shared<MediaVideoDecodeBuffer>(
        std::pmr::polymorphic_allocator<MediaVideoDecodeBuffer>{&allocation}, ConstructionKey{},
        std::move(media), std::move(lineage), std::move(display)));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::allocationFailed("Video decode lineage allocation failed"));
}

::media::Result<MediaBufferRef> MediaVideoDecodeBuffer::withDisplayLineage(
    std::pmr::memory_resource& allocation, MediaBufferRef media,
    std::shared_ptr<const MediaVideoDisplayLineage> display)
try
{
    using Result = ::media::Result<MediaBufferRef>;
    const auto* frame = dynamic_cast<const FFmpegFrameBuffer*>(media.get());
    if (!frame || !frame->frame() || media->streamKind() != MediaStreamKind::Video ||
        frame->frame()->pts == AV_NOPTS_VALUE ||
        media->timeDescriptor().timeBase.num <= 0 || media->timeDescriptor().timeBase.den <= 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Display lineage requires a timed raw video frame"));
    if (auto status = validateMediaCodecLineage(MediaCodecLineage{display}); !status)
        return Result::failure(status.error());
    auto accessUnit = display->accessUnit;
    return Result::success(std::allocate_shared<MediaVideoDecodeBuffer>(
        std::pmr::polymorphic_allocator<MediaVideoDecodeBuffer>{&allocation}, ConstructionKey{},
        std::move(media), std::move(accessUnit), std::move(display)));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::allocationFailed("Transformed video display wrapper allocation failed"));
}

MediaBufferType MediaVideoDecodeBuffer::type() const noexcept { return MediaBufferType::Event; }
std::optional<std::uint64_t> MediaVideoDecodeBuffer::payloadFootprintBytes() const noexcept
{ return m_media->payloadFootprintBytes(); }

} // namespace media::ffmpeg::graph
