#pragma once

#include "internal/graph/model/MediaCapabilityMaturity.h"

namespace media::ffmpeg::graph {

enum class MediaGraphCapability {
    CudaNvencTranscode,
    GraphOptimization,
    GenericGpuExecution,
    DistributedExecution
};

constexpr MediaCapabilityMaturity mediaGraphCapabilityMaturity(MediaGraphCapability capability) noexcept
{
    switch (capability) {
    case MediaGraphCapability::CudaNvencTranscode:
        return MediaCapabilityMaturity::Stable;
    case MediaGraphCapability::GraphOptimization:
    case MediaGraphCapability::GenericGpuExecution:
    case MediaGraphCapability::DistributedExecution:
        return MediaCapabilityMaturity::Unsupported;
    }
    return MediaCapabilityMaturity::Unsupported;
}

} // namespace media::ffmpeg::graph
