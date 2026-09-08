#pragma once

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaDatagramRouteFact final {
    MediaIpAddressFamily addressFamily;
    std::string localNumericAddress;
    std::uint64_t maximumIpPacketBytes;
    std::string serviceScopeId;
    std::string authority;
};

class MediaDatagramRouteProbe final {
public:
    static ::media::Result<MediaDatagramRouteFact> probe(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaDatagramRouteProbe() = delete;
};

} // namespace media::ffmpeg::graph
