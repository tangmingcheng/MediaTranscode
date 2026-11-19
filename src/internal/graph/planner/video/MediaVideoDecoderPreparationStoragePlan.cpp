#include "internal/graph/planner/video/MediaVideoDecoderPreparationStoragePlan.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaVideoDecoderPreparationStoragePlan>
MediaVideoDecoderPreparationStoragePlanner::plan(std::size_t maximumAccessUnitBytes,
    const MediaDecoderInputRetention& retention,
    const CodecResolverDecoderContextBuildResult& openedDecoder,
    const MediaHardwareDescriptor& frameContract)
{
    using Result = ::media::Result<MediaVideoDecoderPreparationStoragePlan>;
    if (!maximumAccessUnitBytes || retention.authority.empty() || retention.threadCount <= 0 ||
        !openedDecoder.context || openedDecoder.context->extradata_size < 0 ||
        frameContract.frameKind == MediaHardwareFrameKind::Unknown)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder preparation storage requires opened codec and bounded input facts"));
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::add(a, b, "source preparation storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const auto multiply = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::multiply(a, b, "source preparation storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const auto retainedPackets = add(add(retention.mainHandoffPackets,
        retention.frameWorkerPackets), retention.serialPrivatePackets);
    const auto decoder = multiply(retainedPackets,
        add(maximumAccessUnitBytes, AV_INPUT_BUFFER_PADDING_SIZE + sizeof(AVPacket)));
    // Count the actual combined format/cancellation callback state;
    // control-block/allocator overhead is outside this logical byte scope.
    auto ownerBytes = add(sizeof(AVCodecContext),
        add(openedDecoder.context->extradata_size, AV_INPUT_BUFFER_PADDING_SIZE));
    // Handoff retains an authoritative source-parameter copy until its single
    // production claimant has verified the immutable input snapshot.
    ownerBytes = add(ownerBytes, add(sizeof(AVCodecParameters),
        add(openedDecoder.context->extradata_size, AV_INPUT_BUFFER_PADDING_SIZE)));
    if (openedDecoder.context->nb_coded_side_data < 0 ||
        (openedDecoder.context->nb_coded_side_data && !openedDecoder.context->coded_side_data))
        return Result::failure(::media::ErrorInfo::invalidArgument("Decoder coded side data is invalid"));
    for (int i = 0; i < openedDecoder.context->nb_coded_side_data; ++i) {
        const auto& side = openedDecoder.context->coded_side_data[i];
        if (side.size && !side.data)
            return Result::failure(::media::ErrorInfo::invalidArgument("Decoder coded side data has no storage"));
        // The decoder and retained source parameters each own this payload.
        ownerBytes = add(ownerBytes, multiply(2, add(sizeof(AVPacketSideData),
            add(side.size, AV_INPUT_BUFFER_PADDING_SIZE))));
    }
    if (openedDecoder.context.get_deleter().callbackOwner)
        ownerBytes = add(ownerBytes, sizeof(MediaVideoDecoderCallbackState));
    // Reopening keeps the device alive after the old codec is destroyed while
    // the common builder acquires its new device reference.
    if (openedDecoder.hardwareDevice) ownerBytes = add(ownerBytes, 2 * sizeof(AVBufferRef));
    if (openedDecoder.context->hw_device_ctx) ownerBytes = add(ownerBytes, sizeof(AVBufferRef));
    std::uint64_t softwareBytes = 0;
    if (frameContract.frameKind == MediaHardwareFrameKind::Software) {
        auto footprint = MediaVideoSurfaceFootprint::logicalBytes(
            frameContract.size.width, frameContract.size.height, frameContract.pixelFormat);
        if (!footprint) return Result::failure(footprint.error());
        softwareBytes = footprint.value();
    }
    if (failure) return Result::failure(failure);
    if (decoder > std::numeric_limits<std::size_t>::max() ||
        ownerBytes > std::numeric_limits<std::size_t>::max() ||
        softwareBytes > std::numeric_limits<std::size_t>::max())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Decoder preparation storage exceeds addressable memory"));
    return Result::success({static_cast<std::size_t>(decoder),
        static_cast<std::size_t>(ownerBytes), static_cast<std::size_t>(softwareBytes)});
}

} // namespace media::ffmpeg::graph
