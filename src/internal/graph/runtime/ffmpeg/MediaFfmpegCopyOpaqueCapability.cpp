#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/version_major.h>
}

namespace media::ffmpeg::graph {

::media::Status validateMediaFfmpegCopyOpaqueCapability(
    bool compileTimeFlagAvailable,
    unsigned compileTimeMajor,
    unsigned runtimeMajor)
{
    if (!compileTimeFlagAvailable) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "FFmpeg COPY_OPAQUE lineage propagation is unavailable in the compile-time headers"));
    }
    if (compileTimeMajor != runtimeMajor) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "FFmpeg COPY_OPAQUE lineage propagation requires matching libavcodec compile-time and runtime major versions"));
    }
    return ::media::Status::success();
}

::media::Status requireMediaFfmpegCopyOpaqueCapability()
{
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
    constexpr bool flagAvailable = true;
#else
    constexpr bool flagAvailable = false;
#endif
    return validateMediaFfmpegCopyOpaqueCapability(
        flagAvailable,
        LIBAVCODEC_VERSION_MAJOR,
        avcodec_version() >> 16U);
}

} // namespace media::ffmpeg::graph
