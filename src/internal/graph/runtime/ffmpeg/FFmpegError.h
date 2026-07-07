#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegErrorString.h"
#include "media_transcode/Result.h"

#include <string>
#include <utility>

namespace media::ffmpeg {

inline ErrorInfo makeError(ErrorCode code,
                           std::string message,
                           int nativeCode = 0)
{
    return ErrorInfo::make(code, std::move(message), nativeCode);
}

inline ErrorInfo makeFFmpegError(std::string operation, int nativeCode)
{
    return ErrorInfo::make(
        ErrorCode::FFmpegFailure,
        std::move(operation) + ": " + errorString(nativeCode),
        nativeCode);
}

inline ErrorInfo makeAllocationError(std::string message)
{
    return ErrorInfo::allocationFailed(std::move(message));
}

} // namespace media::ffmpeg
