#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "media_transcode/Result.h"

#include <string>
#include <span>

namespace media::ffmpeg::graph {

struct MediaRtpVideoParameterSetInfo final {
    MediaSize codedSize;
    MediaRational sampleAspectRatio;
};

class MediaRtpVideoParameterSetValidator final {
public:
    // Complete codec extradata contains only parameter sets; media AUs may
    // omit them, but every set present must match the admitted source facts.
    static ::media::Status validateBinding(std::span<const std::uint8_t> bytes,
        const MediaRtpVideoSignalingFacts& facts, bool parameterSetsOnly);
    static ::media::Result<MediaRtpVideoParameterSetInfo> inspect(
        const std::string& codecName,
        const MediaRtpVideoSignalingFacts& facts);
    static ::media::Status validate(
        const std::string& codecName,
        const MediaRtpVideoSignalingFacts& facts);

private:
    MediaRtpVideoParameterSetValidator() = delete;
};

} // namespace media::ffmpeg::graph
