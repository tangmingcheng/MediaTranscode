#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapterFactory.h"

#include "internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressAdapter.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"
#include "internal/graph/runtime/resource/MediaPreparedAllocationCost.h"
#include "internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressAdapter.h"

namespace media::ffmpeg::graph {

::media::Result<std::size_t> MediaRtpIngressAdapterFactory::preparationStorageBytes(
    const MediaRtpReceiveGeometry& geometry)
{
    using Result = ::media::Result<std::size_t>;
    if (auto status = geometry.validateProduct(); !status)
        return Result::failure(status.error());
    auto adapter = [&]() -> Result {
        switch (geometry.adapterKind()) {
        case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
            return MediaLinuxRtpIngressAdapter::preparationStorageBytes(geometry.descriptorCapacity());
        case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
            return MediaWindowsRtpIngressAdapter::preparationStorageBytes(geometry.descriptorCapacity());
        default:
            return Result::failure(::media::ErrorInfo::unsupported(
                "RTP runtime storage requires an implemented ingress adapter"));
        }
    }();
    return MediaPreparedAllocationCost::sum({adapter,
        MediaRtpIngressStorage::preparationStorageBytes(geometry.batchByteCapacity(),
            geometry.descriptorCapacity(), geometry.requiredBufferAlignmentBytes())});
}

::media::Result<std::size_t> MediaRtpIngressAdapterFactory::maximumPreparationStorageBytes(
    std::size_t wireCapacity, std::size_t maximumDatagramBytes)
{
    using Result = ::media::Result<std::size_t>;
    if (!maximumDatagramBytes || wireCapacity < maximumDatagramBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Ingress preparation requires space for a complete datagram"));
    const auto descriptors = wireCapacity / maximumDatagramBytes;
    // This is a capacity bound, not a fabricated socket capability. The actual
    // geometry additionally intersects it with socket/OS completion limits.
    // Both currently supported adapters require max_align_t arena alignment.
    auto storage = MediaRtpIngressStorage::preparationStorageBytes(
        descriptors * maximumDatagramBytes, descriptors, alignof(std::max_align_t));
#ifdef _WIN32
    auto adapter = MediaWindowsRtpIngressAdapter::preparationStorageBytes(descriptors);
#else
    auto adapter = MediaLinuxRtpIngressAdapter::preparationStorageBytes(descriptors);
#endif
    return MediaPreparedAllocationCost::sum({storage, adapter, storage, adapter,
        MediaPreparedAllocationCost::shared<MediaRtpUdpTransport>()});
}

::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>
MediaRtpIngressAdapterFactory::create(
    std::shared_ptr<MediaRtpUdpTransport> transport,
    const MediaRtpReceiveGeometry& plan,
    MediaLineageAllocationScope allocation)
{
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            status.error());
    }
    switch (plan.adapterKind()) {
    case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
        return MediaLinuxRtpIngressAdapter::create(
            std::move(transport), plan, std::move(allocation));
    case MediaRtpIngressAdapterKind::WindowsRegisteredIo:
    case MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy:
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::unsupported(
                "planner-selected RTP ingress adapter is not implemented"));
    case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
        return MediaWindowsRtpIngressAdapter::create(
            std::move(transport), plan, std::move(allocation));
    default:
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress plan contains an unknown adapter kind"));
    }
}

} // namespace media::ffmpeg::graph
