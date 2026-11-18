#include "internal/graph/runtime/resource/MediaPreparationControl.h"

namespace media::ffmpeg::graph {
::media::Status MediaPreparationControl::check(const char* operation) const
{
    if (cancellation.stop_requested()) return ::media::Status::failure(
        ::media::ErrorInfo::cancelled(std::string(operation) + " was cancelled"));
    if (std::chrono::steady_clock::now() >= deadline) return ::media::Status::failure(
        ::media::ErrorInfo::ioFailure(std::string(operation) + " reached its original deadline"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
