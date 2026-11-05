#include "internal/graph/planner/video/MediaVideoFrameOwnershipStoragePlanner.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <initializer_list>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using Bytes = ::media::Result<std::size_t>;

Bytes addressable(::media::Result<std::uint64_t> value)
{
    if (!value) return Bytes::failure(value.error());
    if (value.value() > (std::numeric_limits<std::size_t>::max)())
        return Bytes::failure(::media::ErrorInfo::allocationFailed(
            "Video ownership metadata exceeds addressable storage"));
    return Bytes::success(static_cast<std::size_t>(value.value()));
}

Bytes sum(std::initializer_list<std::size_t> parts)
{
    std::size_t total = 0;
    for (const auto part : parts) {
        auto next = addressable(MediaCheckedArithmetic::add(total, part, "Video ownership metadata sum"));
        if (!next) return next;
        total = next.value();
    }
    return Bytes::success(total);
}

Bytes multiply(std::size_t count, std::size_t bytes)
{
    return addressable(MediaCheckedArithmetic::multiply(count, bytes, "Video ownership metadata count"));
}

Bytes request(const MediaSharedAllocationGeometry& item, std::size_t count)
{
    if (item.count != 1 || !item.bytes)
        return Bytes::failure(::media::ErrorInfo::invalidArgument("Video ownership requires single-element geometry"));
    auto bytes = multiply(count, item.bytes);
    if (!bytes) return bytes;
    return MediaPreparationMemoryResource::allocationFootprint(bytes.value(), item.alignment);
}

Bytes reservedVector(const MediaSharedAllocationGeometry& element, std::size_t count)
{
    // MSVC 145/202604 vector::reserve on an empty vector calls _Reallocate
    // without geometric growth. In C++20 _Allocate_at_least_helper calls
    // LineageAllocator::allocate(count) exactly once. That allocator and its
    // debug-proxy rebind both charge the same PreparationMemoryResource.
#if defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 145 && _MSVC_STL_UPDATE == 202604L && !_HAS_CXX23
    if (!element.bytes || count > (std::numeric_limits<std::size_t>::max)() / element.bytes ||
        count > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
        return Bytes::failure(::media::ErrorInfo::allocationFailed("Video ownership vector exceeds max_size"));
    auto proxy = MediaPmrContainerGeometry::proxyBytes();
    if (!proxy) return proxy;
    if (!count) return proxy;
    auto backing = request(element, count);
    if (!backing) return backing;
    return sum({proxy.value(), backing.value()});
#elif defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE == 12 && __GLIBCXX__ == 20240620 && \
    __cplusplus == 202002L && !defined(_GLIBCXX_DEBUG)
    // GCC 12.4 vector::reserve allocates exactly count elements; the empty
    // vector owns no allocation or debug proxy. _S_max_size additionally
    // limits the entire backing to PTRDIFF_MAX bytes.
    if (!element.bytes || count > static_cast<std::size_t>(
            (std::numeric_limits<std::ptrdiff_t>::max)()) / element.bytes)
        return Bytes::failure(::media::ErrorInfo::allocationFailed("Video ownership vector exceeds max_size"));
    if (!count) return Bytes::success(0);
    return request(element, count);
#else
    return Bytes::failure(::media::ErrorInfo::unsupported(
        "Video ownership vector storage requires the audited C++20 standard library"));
#endif
}
} // namespace

::media::Result<MediaVideoFrameOwnershipStoragePlan> MediaVideoFrameOwnershipStoragePlanner::plan(
    const MediaVideoFrameOwnershipStorageFacts& facts)
{
    using Result = ::media::Result<MediaVideoFrameOwnershipStoragePlan>;
    if (facts.maximumExtendedBufferReferences > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        (facts.readinessKind != MediaVideoFrameReadiness::Kind::Completed &&
         facts.readinessKind != MediaVideoFrameReadiness::Kind::CudaEvent) ||
        (facts.readinessKind == MediaVideoFrameReadiness::Kind::CudaEvent && !facts.hasHardwareFramesContext))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video ownership storage requires authoritative reference and readiness facts"));
    auto count = addressable(MediaCheckedArithmetic::add(AV_NUM_DATA_POINTERS,
        facts.maximumExtendedBufferReferences, "Video ownership reference capacity"));
    if (!count) return Result::failure(count.error());
    auto layout = describeMediaFfmpegPayloadOwnershipLayout();
    if (!layout) return Result::failure(layout.error());
    const auto& actual = layout.value();
    auto references = request(actual.references, 1);
    auto referenceArray = reservedVector(actual.referenceElement, count.value());
    auto referenceHeaders = multiply(count.value(), sizeof(AVBufferRef));
    auto owner = request(actual.payloadOwner, 1);
    auto replacementArray = reservedVector(actual.replacementElement, count.value());
    if (!references || !referenceArray || !referenceHeaders || !owner || !replacementArray)
        return Result::failure(!references ? references.error() : !referenceArray ? referenceArray.error() :
            !referenceHeaders ? referenceHeaders.error() : !owner ? owner.error() : replacementArray.error());
    auto snapshot = sum({references.value(), referenceArray.value(), referenceHeaders.value(),
        facts.hasHardwareFramesContext ? sizeof(AVBufferRef) : 0});
    // Each attachment owns its original av_buffer_ref and new wrapper reference.
    // Existing upper-layer references and FFmpeg-private AVBuffer are excluded.
    auto attachmentPerReference = sum({owner.value(), sizeof(AVBufferRef), sizeof(AVBufferRef)});
    if (!snapshot || !attachmentPerReference)
        return Result::failure(!snapshot ? snapshot.error() : attachmentPerReference.error());
    auto attachment = multiply(count.value(), attachmentPerReference.value());
    if (!attachment) return Result::failure(attachment.error());
    std::size_t operationBytes = 0;
    std::size_t completionBytes = 0;
    if (facts.readinessKind == MediaVideoFrameReadiness::Kind::CudaEvent) {
        auto operationGeometry = mediaCudaFrameReadinessAllocationGeometry();
        if (!operationGeometry) return Result::failure(operationGeometry.error());
        auto operation = request(operationGeometry.value(), 1);
        auto completion = request(actual.completion, 1);
        if (!operation || !completion) return Result::failure(!operation ? operation.error() : completion.error());
        auto withDeviceReference = sum({operation.value(), sizeof(AVBufferRef)});
        if (!withDeviceReference) return Result::failure(withDeviceReference.error());
        operationBytes = withDeviceReference.value();
        completionBytes = completion.value();
    }
    return Result::success({count.value(), snapshot.value(), attachment.value(), replacementArray.value(),
        operationBytes, completionBytes});
}

} // namespace media::ffmpeg::graph
