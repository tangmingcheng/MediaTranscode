#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

struct MediaScheduledRtpPacketizationPlan final {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::string codecName;
    int streamTimeBaseNumerator = 0;
    int streamTimeBaseDenominator = 0;
    MediaScheduledRtpPacketizationMode packetizationMode =
        MediaScheduledRtpPacketizationMode::H264AnnexB;
    int payloadType = -1;
    std::size_t maximumDatagramBytes = 0;
};

} // namespace media::ffmpeg::graph
