#pragma once
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"
#include <optional>

namespace media::ffmpeg::graph {
enum class MediaVideoSourceAllocation { DecoderOutput, IndependentFilterOutput };
struct MediaVideoSharedSourcePlan final {
    MediaVideoSourceAllocation allocation;
    std::optional<MediaVideoFilterExecutionPlan> copy;
    MediaVideoFilterImplementation copyImplementation;
};
} // namespace media::ffmpeg::graph
