#pragma once

#include "internal/graph/model/MediaZeroCopyPolicy.h"
#include "internal/graph/runtime/zerocopy/MediaZeroCopyCapability.h"
#include "internal/graph/runtime/zerocopy/MediaZeroCopyPlan.h"

namespace media::ffmpeg::graph {

class MediaZeroCopyPlanner final {
public:
    static MediaZeroCopyPlan plan(const MediaHardwareDescriptor& from,
                                  const MediaHardwareDescriptor& to,
                                  const MediaZeroCopyPolicy& policy = {});

private:
    static MediaInteropKind chooseInterop(const MediaHardwareDescriptor& from,
                                          const MediaHardwareDescriptor& to,
                                          const MediaZeroCopyPolicy& policy) noexcept;
};

} // namespace media::ffmpeg::graph
