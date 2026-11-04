#pragma once

#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"
#include <optional>

struct AVBufferRef;

namespace media::ffmpeg::graph {

enum class MediaDecoderOutputAllocationMechanism { NativeCudaSafeCopy, CuvidCopy };

// Independence from fixed decoder surfaces, not a device memory size bound.
struct MediaDecoderOutputAllocation final {
    MediaDecoderOutputAllocationMechanism mechanism;
    MediaDecoderRuntimeFacts decoder;
    MediaHardwareDescriptor frame;
    std::size_t maximumExtendedBufferReferences;
};

class MediaDecoderOutputAllocationAdapter final {
public:
    static ::media::Result<std::optional<MediaDecoderOutputAllocation>> inspect(
        const MediaDecoderRuntimeFacts& decoder, AVBufferRef* frames,
        const MediaHardwareDescriptor& expected);
};

} // namespace media::ffmpeg::graph
