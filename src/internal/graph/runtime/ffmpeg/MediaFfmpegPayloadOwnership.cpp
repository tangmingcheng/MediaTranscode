#include "internal/graph/runtime/ffmpeg/MediaFfmpegPayloadOwnership.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"

#include <new>
#include <vector>
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

namespace media::ffmpeg::graph {
MediaFfmpegPayloadReferences::MediaFfmpegPayloadReferences(ConstructionKey,
    const std::shared_ptr<MediaPreparationMemoryResource>& resource)
    : m_references(MediaLineageAllocator<::media::ffmpeg::BufferRefPtr>(MediaPreparedLineageAllocation{resource})) {}
::media::Result<std::shared_ptr<const MediaFfmpegPayloadReferences>> MediaFfmpegPayloadReferences::capture(
    const AVFrame& frame, const std::shared_ptr<MediaPreparationMemoryResource>& resource)
{
    using Result = ::media::Result<std::shared_ptr<const MediaFfmpegPayloadReferences>>;
    if (!resource || frame.nb_extended_buf < 0 || (frame.nb_extended_buf && !frame.extended_buf))
        return Result::failure(::media::ErrorInfo::invalidArgument("Consumer input allocation snapshot requires resource and valid frame"));
    try {
        auto owner = std::allocate_shared<MediaFfmpegPayloadReferences>(
            MediaLineageAllocator<MediaFfmpegPayloadReferences>(MediaPreparedLineageAllocation{resource}), ConstructionKey{}, resource);
        if (frame.hw_frames_ctx) {
            owner->m_framesContext = ::media::ffmpeg::makeBufferRef(frame.hw_frames_ctx);
            if (!owner->m_framesContext) return Result::failure(
                ::media::ErrorInfo::allocationFailed("Consumer hardware frames context reference"));
        }
        owner->m_references.reserve(AV_NUM_DATA_POINTERS + static_cast<std::size_t>(frame.nb_extended_buf));
        const auto retain = [&owner](AVBufferRef* buffer) {
            if (!buffer) return true;
            auto reference = ::media::ffmpeg::makeBufferRef(buffer);
            if (!reference) return false;
            owner->m_references.push_back(std::move(reference));
            return true;
        };
        for (auto* buffer : frame.buf) if (!retain(buffer))
            return Result::failure(::media::ErrorInfo::allocationFailed("Consumer input allocation reference"));
        for (int i = 0; i < frame.nb_extended_buf; ++i) if (!retain(frame.extended_buf[i]))
            return Result::failure(::media::ErrorInfo::allocationFailed("Consumer extended input allocation reference"));
        if (owner->m_references.empty()) return Result::failure(::media::ErrorInfo::invalidArgument("Consumer input has no allocation references"));
        return Result::success(std::move(owner));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Consumer input snapshot budget exhausted"));
    }
}

MediaFfmpegPayloadCompletion::MediaFfmpegPayloadCompletion(
    ConstructionKey, std::shared_ptr<void> operation, Complete complete, int successCode)
    : m_operation(std::move(operation)), m_complete(complete), m_successCode(successCode) {}

::media::Result<std::shared_ptr<MediaFfmpegPayloadCompletion>> MediaFfmpegPayloadCompletion::create(
    const std::shared_ptr<MediaPreparationMemoryResource>& resource,
    std::shared_ptr<void> operation, Complete complete, int successCode)
{
    using Result = ::media::Result<std::shared_ptr<MediaFfmpegPayloadCompletion>>;
    if (!resource || !operation || !complete)
        return Result::failure(::media::ErrorInfo::invalidArgument("Payload completion requires owned operation and preparation allocator"));
    try {
        return Result::success(std::allocate_shared<MediaFfmpegPayloadCompletion>(
            MediaLineageAllocator<MediaFfmpegPayloadCompletion>(MediaPreparedLineageAllocation{resource}),
            ConstructionKey{}, std::move(operation), complete, successCode));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Payload completion owner"));
    }
}
::media::Status MediaFfmpegPayloadCompletion::ensureCompleted()
{
    std::lock_guard lock(m_mutex);
    if (!m_result) m_result = m_complete(m_operation.get());
    if (*m_result != m_successCode)
        return ::media::Status::failure({::media::ErrorCode::HardwareUnavailable, *m_result,
            "FFmpeg payload producer completion failed"});
    return ::media::Status::success();
}
void MediaFfmpegPayloadCompletion::completeForRelease() noexcept
{
    try {
        const auto status = ensureCompleted();
        if (!status) mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode, status.error().describe());
    } catch (...) {
        // Release remains noexcept; a driver error is never converted into success.
    }
}
MediaFfmpegPayloadCompletion::~MediaFfmpegPayloadCompletion() { completeForRelease(); }

namespace {
::media::Status validateOwnership(const MediaFfmpegPayloadOwnership& ownership)
{
    if ((!ownership.credit && !ownership.completion) ||
        (ownership.credit && !*ownership.credit) ||
        (std::holds_alternative<MediaPreparedLineageAllocation>(ownership.allocation) &&
            !std::get<MediaPreparedLineageAllocation>(ownership.allocation).resource))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "FFmpeg payload ownership requires valid credit or completion and allocation scope"));
    return ::media::Status::success();
}
struct PayloadOwner final {
    MediaLineageAllocationScope scope;
    std::shared_ptr<MediaGraphPayloadCreditLease> credit;
    std::shared_ptr<MediaFfmpegPayloadCompletion> completion;
    ::media::ffmpeg::BufferRefPtr allocation;
    ~PayloadOwner() { if (completion) completion->completeForRelease(); }
};
struct Replacement final {
    AVBufferRef** slot;
    ::media::ffmpeg::BufferRefPtr reference;
};
struct OwnerDeleter final {
    void operator()(PayloadOwner* owner) const noexcept
    {
        if (!owner) return;
        MediaLineageAllocator<PayloadOwner> allocator(owner->scope);
        std::destroy_at(owner);
        allocator.deallocate(owner, 1);
    }
};

void releasePayload(void* opaque, std::uint8_t*) noexcept
{
    OwnerDeleter{}(static_cast<PayloadOwner*>(opaque));
}

::media::Result<::media::ffmpeg::BufferRefPtr> retainAllocation(
    AVBufferRef* original, const MediaFfmpegPayloadOwnership& ownership)
{
    using Result = ::media::Result<::media::ffmpeg::BufferRefPtr>;
    if (!original)
        return Result::failure(::media::ErrorInfo::invalidArgument("FFmpeg payload allocation is absent"));
    ::media::ffmpeg::BufferRefPtr retained(av_buffer_ref(original));
    if (!retained) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Could not retain FFmpeg payload allocation"));
    std::unique_ptr<PayloadOwner, OwnerDeleter> owner;
    try {
        MediaLineageAllocator<PayloadOwner> allocator(ownership.allocation);
        auto* storage = allocator.allocate(1);
        // Every member construction is noexcept; no unowned allocation remains.
        owner.reset(std::construct_at(storage, ownership.allocation, ownership.credit,
            ownership.completion, std::move(retained)));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Could not retain FFmpeg payload allocation owner"));
    }
    // A new AVBuffer identity cannot claim exclusive ownership of aliased data.
    // Mark it read-only so FFmpeg's normal make_writable performs COW when needed.
    ::media::ffmpeg::BufferRefPtr wrapped(av_buffer_create(original->data,
        original->size, releasePayload, owner.get(), AV_BUFFER_FLAG_READONLY));
    if (!wrapped) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Could not create FFmpeg allocation lifetime reference"));
    owner.release();
    return Result::success(std::move(wrapped));
}
} // namespace

::media::Result<MediaFfmpegPayloadOwnershipLayout> describeMediaFfmpegPayloadOwnershipLayout()
{
    using Result = ::media::Result<MediaFfmpegPayloadOwnershipLayout>;
    auto references = probeMediaSharedAllocationGeometry<MediaFfmpegPayloadReferences>(
        std::type_identity<MediaLineageAllocator<MediaFfmpegPayloadReferences>>{});
    if (!references) return Result::failure(references.error());
    auto completion = probeMediaSharedAllocationGeometry<MediaFfmpegPayloadCompletion>(
        std::type_identity<MediaLineageAllocator<MediaFfmpegPayloadCompletion>>{});
    if (!completion) return Result::failure(completion.error());
    return Result::success({references.value(), completion.value(),
        {1, sizeof(PayloadOwner), alignof(PayloadOwner)},
        {1, sizeof(::media::ffmpeg::BufferRefPtr), alignof(::media::ffmpeg::BufferRefPtr)},
        {1, sizeof(Replacement), alignof(Replacement)}});
}

::media::Status retainMediaFfmpegPayload(
    AVPacket& packet, const MediaFfmpegPayloadOwnership& ownership)
{
    if (auto valid = validateOwnership(ownership); !valid) return valid;
    auto retained = retainAllocation(packet.buf, ownership);
    if (!retained) return ::media::Status::failure(retained.error());
    av_buffer_unref(&packet.buf);
    packet.buf = std::move(retained).value().release();
    return ::media::Status::success();
}

::media::Status retainMediaFfmpegPayload(
    AVFrame& frame, const MediaFfmpegPayloadOwnership& ownership)
{
    if (auto valid = validateOwnership(ownership); !valid) return valid;
    if (frame.nb_extended_buf < 0 || (frame.nb_extended_buf && !frame.extended_buf))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "FFmpeg frame has an invalid allocation-reference layout"));
    std::vector<Replacement, MediaLineageAllocator<Replacement>> replacements{
        MediaLineageAllocator<Replacement>(ownership.allocation)};
    try {
        replacements.reserve(AV_NUM_DATA_POINTERS + static_cast<std::size_t>(frame.nb_extended_buf));
        const auto prepare = [&replacements, &ownership](AVBufferRef*& original) -> ::media::Status {
            if (!original) return ::media::Status::success();
            auto retained = retainAllocation(original, ownership);
            if (!retained) return ::media::Status::failure(retained.error());
            replacements.push_back({&original, std::move(retained).value()});
            return ::media::Status::success();
        };
        for (auto& buffer : frame.buf) {
            auto status = prepare(buffer);
            if (!status) return status;
        }
        for (int i = 0; i < frame.nb_extended_buf; ++i) {
            auto status = prepare(frame.extended_buf[i]);
            if (!status) return status;
        }
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "Could not prepare FFmpeg frame allocation ownership"));
    }
    if (replacements.empty()) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "FFmpeg frame payload has no reference-counted allocation"));
    // Transactional replacement; every allocation retains the same lease object,
    // so multiple planes never reserve or release the credit more than once.
    for (auto& replacement : replacements) {
        av_buffer_unref(replacement.slot);
        *replacement.slot = replacement.reference.release();
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
