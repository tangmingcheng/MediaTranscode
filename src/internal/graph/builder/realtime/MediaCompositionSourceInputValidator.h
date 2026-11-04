#pragma once
#include "internal/graph/builder/realtime/MediaRealtimeInputGraphBuilder.h"
namespace media::ffmpeg::graph {
class MediaCompositionSourceInputValidator final {
public:
    static ::media::Status validate(const MediaRealtimeInputGraph& input);
};
}
