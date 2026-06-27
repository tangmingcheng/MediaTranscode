#include "realtime/FFmpegRealtimeVideoPipelineRuntime.h"

#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/core/video/FFmpegVideoProcessingPipeline.h"
#include "internal/graph/packet/PacketOutputGraphController.h"
#include "internal/output/nodes/rtp/FFmpegRtpOutputNode.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media {

// unchanged logic

}