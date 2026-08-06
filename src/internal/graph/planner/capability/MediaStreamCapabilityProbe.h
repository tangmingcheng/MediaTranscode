#pragma once

#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"

struct AVDictionary;

namespace media::ffmpeg::graph {

class MediaStreamCapabilityProbe final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> inspectVideo(
        const std::string& inputUrl, AVDictionary** inputOptions);
    static ::media::Result<MediaRealtimeInputStreamInfo> inspectRealtime(
        const std::string& inputUrl, AVDictionary** inputOptions,
        MediaTranscodeStreamSet streamSet);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepareRealtime(
        const std::string& inputUrl, AVDictionary** inputOptions,
        MediaTranscodeStreamSet streamSet,
        const MediaRealtimeInputOpener& opener);

private:
    MediaStreamCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
