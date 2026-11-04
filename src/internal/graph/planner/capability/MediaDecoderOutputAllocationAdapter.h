#pragma once

#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"
#include <optional>

struct AVBufferRef;

namespace media::ffmpeg::graph {

enum class MediaDecoderOutputAllocationMechanism { NativeCudaSafeCopy, CuvidCopy, RkmppBorrowedSurface };

// Actual publication mechanism, not a device memory size bound. Borrowed
// surfaces retain their decoder pool until all published AVBuffer owners retire.
struct MediaDecoderOutputAllocation final {
    MediaDecoderOutputAllocationMechanism mechanism;
    MediaDecoderRuntimeFacts decoder;
    MediaHardwareDescriptor frame;
    std::size_t maximumExtendedBufferReferences;
    bool independentOfDecoderSurfaces() const noexcept
    {
        return mechanism == MediaDecoderOutputAllocationMechanism::NativeCudaSafeCopy ||
            mechanism == MediaDecoderOutputAllocationMechanism::CuvidCopy;
    }
};

class MediaDecoderOutputAllocationAdapter final {
public:
    static ::media::Result<std::optional<MediaDecoderOutputAllocation>> inspect(
        const MediaDecoderRuntimeFacts& decoder, AVBufferRef* frames,
        const MediaHardwareDescriptor& expected);
};

} // namespace media::ffmpeg::graph
