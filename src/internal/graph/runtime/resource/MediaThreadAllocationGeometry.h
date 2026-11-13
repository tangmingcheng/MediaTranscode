#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
#include <stop_token>

namespace media::ffmpeg::graph {
// One fixed, allocation-free invocation type makes the STL startup allocation
// authoritative for every prepared receiver worker.
struct MediaThreadWorkerEntry final {
    using Function = void (*)(void*, std::stop_token, std::size_t) noexcept;
    Function function;
    void* context;
    std::size_t index;
    void operator()(std::stop_token token) const noexcept
    { function(context, token, index); }
};

struct MediaThreadInvocationGeometry final {
    std::size_t bytes;
    std::size_t alignment;
};

struct MediaThreadAllocationGeometry final {
    std::size_t invocationBytes;
    std::size_t invocationAlignment;
    std::size_t stopStateBytes;
    std::size_t stopStateAlignment;
    static ::media::Result<MediaThreadAllocationGeometry> current();
    static ::media::Result<MediaThreadInvocationGeometry> graphWorker();
};
} // namespace media::ffmpeg::graph
