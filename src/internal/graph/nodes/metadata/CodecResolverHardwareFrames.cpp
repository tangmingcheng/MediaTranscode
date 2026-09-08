#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

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

    AVBufferRef* framesRef = av_hwframe_ctx_alloc(hardwareDevice);
    if (!framesRef) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("av_hwframe_ctx_alloc returned null"));
    }

    AVHWFramesContext* framesContext = reinterpret_cast<AVHWFramesContext*>(framesRef->data);
    framesContext->format = hardwareFormat;
    framesContext->sw_format = softwareFormat;
    framesContext->width = width;
    framesContext->height = height;
    framesContext->initial_pool_size = initialPoolSize;

    const int initRet = av_hwframe_ctx_init(framesRef);
    if (initRet < 0) {
        av_buffer_unref(&framesRef);
        return FFmpegGraphError::statusFromCode(initRet, "av_hwframe_ctx_init(encoder)");
    }
    if (auto status = validateEncoderHardwareFramesPoolReadback(
            initialPoolSize, framesContext->initial_pool_size,
            poolAuthority); !status) {
        av_buffer_unref(&framesRef);
        return status;
    }

    encoderContext->hw_frames_ctx = framesRef;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
