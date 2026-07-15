#include "internal/graph/model/MediaOutputResourceKind.h"

namespace media::ffmpeg::graph {

::media::Result<std::string> mediaOutputResourceKindOptionValue(
    MediaOutputResourceKind kind)
{
    switch (kind) {
    case MediaOutputResourceKind::FFmpegFormatContext:
        return ::media::Result<std::string>::success("ffmpeg_format_context");
    case MediaOutputResourceKind::ByteSink:
        return ::media::Result<std::string>::success("byte_sink");
    }
    return ::media::Result<std::string>::failure(
        ::media::ErrorInfo::unsupported("unsupported output resource kind"));
}

::media::Result<MediaOutputResourceKind> parseMediaOutputResourceKindOption(
    std::string_view value)
{
    if (value == "ffmpeg_format_context") {
        return ::media::Result<MediaOutputResourceKind>::success(
            MediaOutputResourceKind::FFmpegFormatContext);
    }
    if (value == "byte_sink") {
        return ::media::Result<MediaOutputResourceKind>::success(
            MediaOutputResourceKind::ByteSink);
    }
    return ::media::Result<MediaOutputResourceKind>::failure(
        ::media::ErrorInfo::invalidArgument("unknown output resource kind"));
}

} // namespace media::ffmpeg::graph
