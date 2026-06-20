#pragma once

#include "internal/FFmpegVideoColorMetadata.h"

extern "C" {
#include <libavfilter/avfilter.h>
}

namespace media::ffmpeg {

    struct BufferSourceMetadataApplyReport {
        bool colorRangeApplied = false;
        bool colorSpaceApplied = false;
        bool colorPrimariesApplied = false;
        bool colorTransferApplied = false;
        bool chromaLocationApplied = false;

        bool anyApplied() const;
    };

    class BufferSourceMetadataApplier {
    public:
        static BufferSourceMetadataApplyReport apply(AVFilterContext* bufferSourceContext,
                                                     const VideoColorMetadata& metadata);
    };

} // namespace media::ffmpeg
