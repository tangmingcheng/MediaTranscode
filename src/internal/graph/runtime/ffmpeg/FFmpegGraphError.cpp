#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
}

#include <array>
#include <utility>

namespace media::ffmpeg::graph {

::media::ErrorInfo FFmpegGraphError::fromCode(int code, std::string operation)
{
    if (code >= 0) {
        return ::media::ErrorInfo::success();
    }

    std::string message = std::move(operation);
    if (!message.empty()) {
        message += " failed: ";
    }
    message += describe(code);

    return ::media::ErrorInfo::ffmpegFailure(std::move(message), code);
}

::media::Status FFmpegGraphError::statusFromCode(int code, std::string operation)
{
    if (code >= 0) {
        return ::media::Status::success();
    }

    return ::media::Status::failure(fromCode(code, std::move(operation)));
}

std::string FFmpegGraphError::describe(int code)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    av_strerror(code, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

} // namespace media::ffmpeg::graph
