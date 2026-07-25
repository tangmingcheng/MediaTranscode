#include "internal/graph/nodes/demux/DemuxReadFailureClassifier.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg::graph {

::media::Status classifyDemuxReadFailure(int ffmpegCode, bool interruptRequested)
{
    if (ffmpegCode == AVERROR_EXIT && interruptRequested) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled("DemuxNode av_read_frame interrupted"));
    }
    return FFmpegGraphError::statusFromCode(ffmpegCode, "av_read_frame");
}

} // namespace media::ffmpeg::graph
