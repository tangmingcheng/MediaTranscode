#include "internal/graph/runtime/ffmpeg/MediaFfmpegPayloadOwnership.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <new>
#include <vector>

namespace media::ffmpeg::graph {
namespace {
struct PayloadOwner final {
    std::shared_ptr<MediaGraphPayloadCreditLease> credit;
    ::media::ffmpeg::BufferRefPtr allocation;
};

void releasePayload(void* opaque, std::uint8_t*) noexcept
{
    delete static_cast<PayloadOwner*>(opaque);
}

::media::Result<::media::ffmpeg::BufferRefPtr> retainAllocation(
    AVBufferRef* original, const std::shared_ptr<MediaGraphPayloadCreditLease>& credit)
{
    using Result = ::media::Result<::media::ffmpeg::BufferRefPtr>;
    if (!original || !credit || !*credit)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "FFmpeg payload ownership requires a reference-counted allocation and credit"));
    ::media::ffmpeg::BufferRefPtr retained(av_buffer_ref(original));
    if (!retained) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Could not retain FFmpeg payload allocation"));
    auto owner = std::unique_ptr<PayloadOwner>(new (std::nothrow) PayloadOwner{
        credit, std::move(retained)});
    if (!owner) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Could not retain FFmpeg payload credit owner"));
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

::media::Status retainMediaFfmpegPayload(
    AVPacket& packet, const std::shared_ptr<MediaGraphPayloadCreditLease>& credit)
{
    auto retained = retainAllocation(packet.buf, credit);
    if (!retained) return ::media::Status::failure(retained.error());
    av_buffer_unref(&packet.buf);
    packet.buf = std::move(retained).value().release();
    return ::media::Status::success();
}

::media::Status retainMediaFfmpegPayload(
    AVFrame& frame, const std::shared_ptr<MediaGraphPayloadCreditLease>& credit)
{
    if (frame.nb_extended_buf < 0 || (frame.nb_extended_buf && !frame.extended_buf))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "FFmpeg frame has an invalid allocation-reference layout"));
    struct Replacement final {
        AVBufferRef** slot;
        ::media::ffmpeg::BufferRefPtr reference;
    };
    std::vector<Replacement> replacements;
    try {
        replacements.reserve(AV_NUM_DATA_POINTERS + static_cast<std::size_t>(frame.nb_extended_buf));
        const auto prepare = [&replacements, &credit](AVBufferRef*& original) -> ::media::Status {
            if (!original) return ::media::Status::success();
            auto retained = retainAllocation(original, credit);
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
