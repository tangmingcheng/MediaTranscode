#pragma once
#include "media_transcode/Result.h"
#include <string_view>

namespace media::ffmpeg::graph {
// Cleanup must retain the failure category and native code even when memory
// pressure prevents retaining its diagnostic text. This does not skip cleanup.
inline ::media::ErrorInfo mediaCleanupError(
    ::media::ErrorCode code, std::string_view message, int nativeCode) noexcept
{
    try { return {code, nativeCode, std::string(message)}; }
    catch (...) { return {code, nativeCode, {}}; }
}
inline ::media::ErrorInfo mediaCleanupError(const ::media::ErrorInfo& error) noexcept
{
    return mediaCleanupError(error.code, error.message, error.nativeCode);
}
} // namespace media::ffmpeg::graph
