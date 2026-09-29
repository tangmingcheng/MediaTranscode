#pragma once

#include "media_transcode/Result.h"
#include <chrono>
#include <stop_token>
#include <string>

namespace media::ffmpeg::graph {

// Cooperative checks around synchronous platform calls; cannot interrupt a
// driver call in progress. Every phase uses the original request deadline.
struct MediaPreparationControl final {
    std::chrono::steady_clock::time_point deadline;
    std::stop_token cancellation;
    ::media::Status check(const char* operation) const;
};

} // namespace media::ffmpeg::graph
