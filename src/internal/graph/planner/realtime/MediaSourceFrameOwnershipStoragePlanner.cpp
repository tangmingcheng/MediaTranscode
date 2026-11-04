#include "internal/graph/planner/realtime/MediaSourceFrameOwnershipStoragePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

namespace media::ffmpeg::graph {
::media::Result<MediaSourceFrameOwnershipStoragePlan> MediaSourceFrameOwnershipStoragePlanner::plan(
    const MediaVideoFrameOwnershipStoragePlan& decoder,
    const std::optional<MediaVideoFrameOwnershipStoragePlan>& filter,
    std::uint64_t decoderReferenceBound, std::optional<std::uint64_t> filterReferenceBound)
{
    using Result = ::media::Result<MediaSourceFrameOwnershipStoragePlan>;
    using Arithmetic = MediaCheckedArithmetic;
    if (!decoderReferenceBound || filter.has_value() != filterReferenceBound.has_value() ||
        (filterReferenceBound && !*filterReferenceBound))
        return Result::failure(::media::ErrorInfo::invalidArgument("Frame ownership requires actual producer residence bounds"));
    // Each node executes one capture at a time. The graph reference bound can
    // already include its constructing output; counting this again is deliberate
    // conservatism, not a claim of that many distinct frames.
    constexpr std::uint64_t captureInProgress = 1;
    constexpr std::uint64_t filterPendingInput = 1;
    constexpr std::uint64_t filterCaptureTemporary = 1;
    MediaSourceFrameOwnershipStoragePlan result{};
    if (filter) {
        auto operations = Arithmetic::add(*filterReferenceBound, captureInProgress, "filter readiness owners");
        if (!operations) return Result::failure(operations.error());
        result.filterOperations = operations.value();
        // complete() does not drop consumedInputs_. Every live output operation
        // can retain one snapshot, in addition to pending and construction slots.
        auto snapshots = Arithmetic::add(operations.value(), filterPendingInput + filterCaptureTemporary,
            "filter retained input snapshots");
        if (!snapshots) return Result::failure(snapshots.error());
        result.inputSnapshots = snapshots.value();
    }
    auto decoderOwners = Arithmetic::add(decoderReferenceBound, result.inputSnapshots,
        "decoder owners retained by filter snapshots");
    if (!decoderOwners) return Result::failure(decoderOwners.error());
    decoderOwners = Arithmetic::add(decoderOwners.value(), captureInProgress, "decoder capture owner");
    if (!decoderOwners) return Result::failure(decoderOwners.error());
    result.decoderOperations = decoderOwners.value();
    const auto charge = [&](std::uint64_t bytes, std::uint64_t count) -> ::media::Status {
        auto product = Arithmetic::multiply(bytes, count, "frame ownership concurrent storage");
        if (!product) return ::media::Status::failure(product.error());
        auto total = Arithmetic::add(result.bytes, product.value(), "source frame ownership storage");
        if (!total) return ::media::Status::failure(total.error());
        result.bytes = total.value();
        return ::media::Status::success();
    };
    const auto producer = [&](const MediaVideoFrameOwnershipStoragePlan& geometry, std::uint64_t count) -> ::media::Status {
        for (const auto bytes : {geometry.payloadAttachmentBytes, geometry.readinessOperationBytes, geometry.completionBytes})
            if (auto status = charge(bytes, count); !status) return status;
        return charge(geometry.payloadAttachmentScratchBytes, captureInProgress);
    };
    if (auto status = producer(decoder, result.decoderOperations); !status) return Result::failure(status.error());
    if (filter) {
        if (auto status = producer(*filter, *result.filterOperations); !status) return Result::failure(status.error());
        if (auto status = charge(decoder.referenceSnapshotBytes, result.inputSnapshots); !status) return Result::failure(status.error());
    }
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
