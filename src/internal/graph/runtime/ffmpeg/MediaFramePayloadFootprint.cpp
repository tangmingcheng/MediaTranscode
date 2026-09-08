#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> videoBytes(const AVFrame& frame)
{
    int width = frame.width;
    int height = frame.height;
    auto format = static_cast<AVPixelFormat>(frame.format);
    if (frame.hw_frames_ctx) {
        const auto* frames = reinterpret_cast<const AVHWFramesContext*>(
            frame.hw_frames_ctx->data);
        if (!frames || frames->sw_format == AV_PIX_FMT_NONE) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::notInitialized(
                    "hardware frame lacks an authoritative software surface format"));
        }
        format = frames->sw_format;
        if (width <= 0) width = frames->width;
        if (height <= 0) height = frames->height;
    }
    if (width <= 0 || height <= 0 || format == AV_PIX_FMT_NONE) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "video frame lacks logical footprint geometry"));
    }
    const int bytes = av_image_get_buffer_size(format, width, height, 1);
    if (bytes <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            FFmpegGraphError::fromCode(
                bytes, "av_image_get_buffer_size(frame payload)"));
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(bytes));
}

::media::Result<std::uint64_t> audioBytes(const AVFrame& frame)
{
    const int channels = frame.ch_layout.nb_channels;
    if (channels <= 0 || frame.nb_samples <= 0 ||
        frame.format == AV_SAMPLE_FMT_NONE) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "audio frame lacks logical footprint geometry"));
    }
    const int bytes = av_samples_get_buffer_size(
        nullptr, channels, frame.nb_samples,
        static_cast<AVSampleFormat>(frame.format), 1);
    if (bytes <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            FFmpegGraphError::fromCode(
                bytes, "av_samples_get_buffer_size(frame payload)"));
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(bytes));
}

} // namespace

::media::Result<std::uint64_t> MediaFramePayloadFootprint::logicalBytes(
    const AVFrame& frame,
    MediaStreamKind streamKind)
{
    if (streamKind == MediaStreamKind::Video) return videoBytes(frame);
    if (streamKind == MediaStreamKind::Audio) return audioBytes(frame);
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::invalidArgument(
            "frame payload footprint requires an audio or video stream"));
}

} // namespace media::ffmpeg::graph
