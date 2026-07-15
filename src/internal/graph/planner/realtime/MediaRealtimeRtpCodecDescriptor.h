#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/audio/MediaAudioProfile.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpCodecDescriptor {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::string codecName;
    std::string rtpEncodingName;
    int clockRate = 0;
    int channels = 0;
    int accessUnitDurationRtpTicks = 0;
    int maximumAccessUnitDurationRtpTicks = 0;
    MediaAudioProfile audioProfile = MediaAudioProfile::notApplicable();
    bool requiresFmtp = false;
};

class MediaRealtimeRtpCodecRegistry final {
public:
    static ::media::Result<MediaRealtimeRtpCodecDescriptor> describe(
        MediaStreamKind streamKind,
        const MediaRealtimeRtpInputMetadata& metadata);

private:
    MediaRealtimeRtpCodecRegistry() = default;
};

} // namespace media::ffmpeg::graph
