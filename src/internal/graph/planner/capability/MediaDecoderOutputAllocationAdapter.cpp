#include "internal/graph/planner/capability/MediaDecoderOutputAllocationAdapter.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

#include <new>

namespace media::ffmpeg::graph {

::media::Result<std::optional<MediaDecoderOutputAllocation>> MediaDecoderOutputAllocationAdapter::inspect(
    const MediaDecoderRuntimeFacts& decoder, AVBufferRef* frames,
    const MediaHardwareDescriptor& expected)
try {
    using Result = ::media::Result<std::optional<MediaDecoderOutputAllocation>>;
    if (decoder.decoderName.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Decoder output allocation requires opened decoder identity"));
    if (!expected.isHardwareBacked() || expected.deviceKind != MediaHardwareDeviceKind::CUDA)
        return Result::success(std::nullopt);
    auto valid = MediaVideoFrameContractValidator::validateHardwareFrames(frames, expected,
        "decoder output allocation");
    if (!valid) return Result::failure(valid.error());
    const auto* context = reinterpret_cast<const AVHWFramesContext*>(frames->data);
    if (context->device_ref->size < sizeof(AVHWDeviceContext) ||
        reinterpret_cast<const AVHWDeviceContext*>(context->device_ref->data)->type != AV_HWDEVICE_TYPE_CUDA)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder CUDA output has a different hardware device type"));
    // A CUDA device alone is insufficient: opaque output and externally supplied
    // or fixed output pools do not have the dynamic copied-output contract.
    if (context->format != AV_PIX_FMT_CUDA || context->initial_pool_size != 0 ||
        decoder.suppliedHardwareFramesContext)
        return Result::success(std::nullopt);
    const bool cuvid = decoder.decoderName == "h264_cuvid" || decoder.decoderName == "hevc_cuvid";
    const bool native = (decoder.decoderName == "h264" || decoder.decoderName == "hevc") &&
        (decoder.hardwareAccelerationFlags & AV_HWACCEL_FLAG_UNSAFE_OUTPUT) == 0;
    if (!cuvid && !native) return Result::success(std::nullopt);
    // nvdec_retrieve_data makes mapped read-only frames writable; decode.c
    // releases private_ref before publication. cuvid_output_frame copies and
    // unmaps explicitly. Neither retains its fixed decode surface downstream.
    // Both copied outputs use av_hwframe_get_buffer's CUDA allocator, whose
    // cuda_get_buffer stores data in buf[0]. No engine-added canvas lease is
    // attached in the decoder producer. This is a producer contract, not a
    // bound inferred from the first observed frame (libavutil/hwcontext_cuda.c).
    return Result::success(MediaDecoderOutputAllocation{cuvid
        ? MediaDecoderOutputAllocationMechanism::CuvidCopy
        : MediaDecoderOutputAllocationMechanism::NativeCudaSafeCopy, decoder, expected, 0});
} catch (const std::bad_alloc&) {
    return ::media::Result<std::optional<MediaDecoderOutputAllocation>>::failure(
        ::media::ErrorInfo::allocationFailed("Decoder output allocation evidence"));
}

} // namespace media::ffmpeg::graph
