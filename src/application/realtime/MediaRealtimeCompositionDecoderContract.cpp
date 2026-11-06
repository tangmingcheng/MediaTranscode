#include "application/realtime/MediaRealtimeCompositionDecoderContract.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <limits>
#include <memory_resource>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::size_t> metadataBytes(const MediaVideoSourcePlan& candidate)
{
    using Result = ::media::Result<std::size_t>;
    std::size_t total = 0;
    const auto add = [&](const std::string& value) -> ::media::Status {
        auto bytes = MediaStandardContainerStorageReadback::stringCopy(value.size());
        if (!bytes) return ::media::Status::failure(bytes.error());
        auto sum = MediaCheckedArithmetic::add(total, bytes.value(), "composition decoder contract strings");
        if (!sum) return ::media::Status::failure(sum.error());
        if (sum.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Decoder contract metadata overflows"));
        total = static_cast<std::size_t>(sum.value());
        return ::media::Status::success();
    };
    for (const auto* text : {&candidate.decoder.ffmpegName, &candidate.decoder.hwaccelName, &candidate.filter.filterName})
        if (auto status = add(*text); !status) return Result::failure(status.error());
    if (candidate.decoder.preparedInputRetention) {
        if (auto status = add(candidate.decoder.preparedInputRetention->authority); !status)
            return Result::failure(status.error());
    }
    for (const auto* descriptor : {&candidate.decoder.outputFrame, &candidate.filter.inputFrame, &candidate.filter.outputFrame}) {
        if (!*descriptor) continue;
        const auto& frame = **descriptor;
        for (const auto* text : {&frame.deviceName, &frame.pixelFormat, &frame.surfacePixelFormat, &frame.framesContextName})
            if (auto status = add(*text); !status) return Result::failure(status.error());
    }
    return Result::success(total);
}
} // namespace

MediaRealtimeCompositionDecoderContract::MediaRealtimeCompositionDecoderContract(
    ConstructionKey, const MediaVideoSourcePlan& candidate,
    std::optional<MediaPreparationStorageLease> metadataStorage)
    : m_metadataStorage(std::move(metadataStorage)), m_decoderName(candidate.decoder.ffmpegName),
      m_hardwareName(candidate.decoder.hwaccelName), m_decoderRetention(candidate.decoder.preparedInputRetention),
      m_decoderFrame(candidate.decoder.outputFrame), m_decoderLineage(candidate.decoderLineagePropagation),
      m_filterActive(candidate.filterActive), m_filterName(candidate.filter.filterName),
      m_filterInputFrame(candidate.filter.inputFrame), m_filterOutputFrame(candidate.filter.outputFrame),
      m_transfer(candidate.transferDirection) {}

::media::Result<std::shared_ptr<const MediaRealtimeCompositionDecoderContract>>
MediaRealtimeCompositionDecoderContract::create(const MediaVideoSourcePlan& candidate,
    const std::shared_ptr<MediaPreparationStorageBudget>& root)
try {
    using Result = ::media::Result<std::shared_ptr<const MediaRealtimeCompositionDecoderContract>>;
    auto bytes = metadataBytes(candidate);
    if (!bytes) return Result::failure(bytes.error());
    auto resource = MediaPreparationMemoryResource::create(root);
    if (!resource) return Result::failure(resource.error());
    std::optional<MediaPreparationStorageLease> storage;
    if (bytes.value()) {
        auto reserved = resource.value()->reserveStorage(bytes.value(), MediaStorageAdmission::RequiredAllocation);
        if (!reserved) return Result::failure(reserved.error());
        storage.emplace(std::move(reserved).value());
    }
    // The resource's allocation header retains its owner through the last weak
    // control-block owner, independently of the object's ordinary-string lease.
    return Result::success(std::allocate_shared<MediaRealtimeCompositionDecoderContract>(
        std::pmr::polymorphic_allocator<MediaRealtimeCompositionDecoderContract>{resource.value().get()},
        ConstructionKey{}, candidate, std::move(storage)));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<const MediaRealtimeCompositionDecoderContract>>::failure(
        ::media::ErrorInfo::allocationFailed("Composition decoder contract allocation failed"));
}

bool MediaRealtimeCompositionDecoderContract::matches(const MediaVideoSourcePlan& candidate) const
{
    return m_decoderName == candidate.decoder.ffmpegName &&
        m_hardwareName == candidate.decoder.hwaccelName &&
        m_decoderRetention == candidate.decoder.preparedInputRetention &&
        m_decoderFrame == candidate.decoder.outputFrame &&
        m_decoderLineage == candidate.decoderLineagePropagation &&
        m_filterActive == candidate.filterActive && m_filterName == candidate.filter.filterName &&
        m_filterInputFrame == candidate.filter.inputFrame && m_filterOutputFrame == candidate.filter.outputFrame &&
        m_transfer == candidate.transferDirection;
}
} // namespace media::ffmpeg::graph
