#pragma once

#include "internal/graph/planner/video/MediaPipelineStagePlan.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

struct AVBufferRef;

namespace media::ffmpeg::graph {

struct MediaVideoEncoderProbeInput final {
    MediaRational sampleAspectRatio;
    std::optional<AVColorRange> colorRange;
    AVBufferRef* hardwareDevice;
    AVBufferRef* hardwareFrames;
    std::string backend;
    std::optional<AVPixelFormat> advertisedEquivalentSoftwareSurface;
};

// Opens and consumes a disposable encoder using caller-owned frame resources.
// It neither allocates a frame pool nor returns a context for production use.
class MediaVideoEncoderCapabilityProbe final {
public:
    static ::media::Status inspect(
        MediaPipelineStagePlan& encoder, const MediaVideoEncoderProbeInput& input);

private:
    MediaVideoEncoderCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
