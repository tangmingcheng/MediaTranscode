#pragma once

#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"

struct AVDictionary;

namespace media::ffmpeg::graph {

class MediaStreamCapabilityProbe final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> inspectVideo(
        const std::string& inputUrl, AVDictionary** inputOptions);
    static ::media::Result<MediaRealtimeInputStreamInfo> inspectRealtime(
        const std::string& inputUrl, AVDictionary** inputOptions, bool includeAudio);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepareRealtime(
        const std::string& inputUrl, AVDictionary** inputOptions, bool includeAudio,
        const MediaRealtimeInputOpener& opener);

private:
    MediaStreamCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
