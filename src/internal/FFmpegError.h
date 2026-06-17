#pragma once

#include "internal/FFmpegUtils.h"
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
        nativeCode
    );
}

inline ErrorInfo makeAllocationError(std::string message)
{
    return ErrorInfo::allocationFailed(std::move(message));
}

inline ErrorInfo makeLegacyError(std::string message,
                                 ErrorCode code = ErrorCode::InternalError)
{
    if (message.empty()) {
        message = "unknown ffmpeg pipeline error";
    }
    return ErrorInfo::make(code, std::move(message));
}

inline Status makeLegacyStatus(bool ok,
                               const std::string& message,
                               ErrorCode code = ErrorCode::InternalError)
{
    if (ok) {
        return Status::success();
    }
    return Status::failure(makeLegacyError(message, code));
}

inline void writeLegacyError(const ErrorInfo& error, std::string* legacyError)
{
    if (legacyError) {
        *legacyError = error.message;
    }
}

inline bool toLegacyBool(const Status& status, std::string* legacyError)
{
    if (status) {
        return true;
    }

    writeLegacyError(status.error(), legacyError);
    return false;
}

} // namespace media::ffmpeg
