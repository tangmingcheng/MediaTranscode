#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"

#include "internal/graph/runtime/ffmpeg/MediaHardwareFramesAllocation.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace media::ffmpeg::graph {

::media::Status validateEncoderHardwareFramesPoolReadback(
    int requestedInitialPoolSize,
    int effectiveInitialPoolSize,
    const char* authority)
{
    if (requestedInitialPoolSize <= 0 || effectiveInitialPoolSize <= 0 ||
        !authority || authority[0] == '\0') {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "hardware frames pool readback requires a positive planner contract and authority"));
    }
    if (effectiveInitialPoolSize != requestedInitialPoolSize) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "hardware frames pool readback conflicts with the planner contract"));
    }
    return ::media::Status::success();
}

::media::Status configureEncoderHardwareFrames(AVCodecContext* encoderContext,
                                               AVBufferRef* hardwareDevice,
                                               AVPixelFormat hardwareFormat,
                                               AVPixelFormat softwareFormat,
                                               int width,
                                               int height,
                                               int initialPoolSize,
                                               const char* poolAuthority)
{
    if (!encoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("configureEncoderHardwareFrames requires encoder context"));
    }

    if (!hardwareDevice) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("configureEncoderHardwareFrames requires hardware device"));
    }

    if (hardwareFormat == AV_PIX_FMT_NONE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("configureEncoderHardwareFrames requires hardware pixel format"));
    }

    if (softwareFormat == AV_PIX_FMT_NONE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("configureEncoderHardwareFrames requires software pixel format"));
    }

    if (width <= 0 || height <= 0 || initialPoolSize <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "configureEncoderHardwareFrames requires positive dimensions and a planner-owned pool size"));
    }

    encoderContext->hw_device_ctx = av_buffer_ref(hardwareDevice);
    if (!encoderContext->hw_device_ctx) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("av_buffer_ref(encoder hw_device_ctx) returned null"));
    }

    auto frames = allocateHardwareFramesContext(hardwareDevice, hardwareFormat, softwareFormat,
        width, height, initialPoolSize);
    if (!frames) return ::media::Status::failure(frames.error());
    const auto* framesContext = reinterpret_cast<const AVHWFramesContext*>(frames.value()->data);
    if (auto status = validateEncoderHardwareFramesPoolReadback(initialPoolSize,
            framesContext->initial_pool_size, poolAuthority); !status) return status;
    encoderContext->hw_frames_ctx = std::move(frames).value().release();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
