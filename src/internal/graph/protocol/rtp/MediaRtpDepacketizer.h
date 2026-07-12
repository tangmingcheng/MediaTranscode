#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpDepacketizerConfig final {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::string codecName;
    std::string fmtp;
    uint8_t payloadType = 0;
    int clockRate = 0;
    int channels = 0;
    int accessUnitDurationRtpTicks = 0;
};

struct MediaRtpAccessUnit final {
    ::media::ffmpeg::PacketPtr packet;
    uint32_t rtpTimestamp = 0;
    MediaRational timeBase;
};

struct MediaRtpDepacketizerResult final {
    std::vector<MediaRtpAccessUnit> accessUnits;
};

class MediaRtpDepacketizer {
public:
    virtual ~MediaRtpDepacketizer() = default;
    virtual ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) = 0;
    virtual void discontinuity(MediaRtpDiscontinuityReason reason) noexcept = 0;
};

::media::Result<MediaRtpAccessUnit> makeRtpAccessUnit(std::vector<uint8_t> bytes,
                                                      uint32_t rtpTimestamp,
                                                      int clockRate,
                                                      int64_t duration,
                                                      bool keyFrame);

} // namespace media::ffmpeg::graph
