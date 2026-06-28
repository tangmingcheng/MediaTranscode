#pragma once

#include "internal/graph/model/MediaZeroCopyPolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/zerocopy/MediaZeroCopyPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaZeroCopyRuntimeSession final {
public:
    void setPolicy(MediaZeroCopyPolicy policy) noexcept;
    const MediaZeroCopyPolicy& policy() const noexcept;

    ::media::Result<MediaBufferRef> process(const MediaBufferRef& input,
                                             const MediaZeroCopyPlan& plan);

private:
    MediaZeroCopyPolicy m_policy;
};

} // namespace media::ffmpeg::graph
