#include "internal/graph/runtime/ffmpeg/MediaRkmppDecoderCancellationAdapter.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCallbackState.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace media::ffmpeg::graph {
::media::Status MediaRkmppDecoderCancellationAdapter::registerCallback(
    AVCodecContext* context, MediaVideoDecoderCallbackState& owner)
{
#if defined(__linux__)
    using Query = unsigned (*)(AVCodecContext*);
    using Register = int (*)(AVCodecContext*, unsigned, int (*)(void*), void*);
    const auto query = reinterpret_cast<Query>(dlsym(RTLD_DEFAULT, "avcodec_rkmpp_interrupt_version"));
    const auto install = reinterpret_cast<Register>(dlsym(RTLD_DEFAULT, "avcodec_rkmpp_set_interrupt_callback"));
    if (!query || !install || query(context) != 1)
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "RKMPP decoder cooperative cancellation ABI v1 is unavailable; paired MPP dependency validation is required separately"));
    const int ret = install(context, 1, &MediaVideoDecoderCallbackState::interruptCallback, &owner);
    if (ret < 0) return ::media::Status::failure(
        FFmpegGraphError::fromCode(ret, "RKMPP decoder cancellation registration"));
    return ::media::Status::success();
#else
    (void)context;
    (void)owner;
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "RKMPP decoder cancellation requires the Linux backend adapter"));
#endif
}
} // namespace media::ffmpeg::graph
