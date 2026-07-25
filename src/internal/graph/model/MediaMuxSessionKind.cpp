#include "internal/graph/model/MediaMuxSessionKind.h"

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view FFmpegFileOption = "ffmpeg_file";
constexpr std::string_view ProjectMpegTsOption = "project_mpegts";

} // namespace

::media::Result<std::string> mediaMuxSessionKindOptionValue(
    MediaMuxSessionKind kind)
{
    switch (kind) {
    case MediaMuxSessionKind::FFmpegFile:
        return ::media::Result<std::string>::success(
            std::string(FFmpegFileOption));
    case MediaMuxSessionKind::ProjectMpegTs:
        return ::media::Result<std::string>::success(
            std::string(ProjectMpegTsOption));
    }
    return ::media::Result<std::string>::failure(
        ::media::ErrorInfo::invalidArgument(
            "unknown media mux session kind"));
}

::media::Result<MediaMuxSessionKind> parseMediaMuxSessionKindOption(
    std::string_view value)
{
    if (value == FFmpegFileOption) {
        return ::media::Result<MediaMuxSessionKind>::success(
            MediaMuxSessionKind::FFmpegFile);
    }
    if (value == ProjectMpegTsOption) {
        return ::media::Result<MediaMuxSessionKind>::success(
            MediaMuxSessionKind::ProjectMpegTs);
    }
    return ::media::Result<MediaMuxSessionKind>::failure(
        ::media::ErrorInfo::invalidArgument(
            "unknown mux.session_kind option value"));
}

} // namespace media::ffmpeg::graph
