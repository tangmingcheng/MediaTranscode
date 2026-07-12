#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <cstring>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpAccessUnit> makeRtpAccessUnit(std::vector<uint8_t> bytes,
                                                      uint32_t rtpTimestamp,
                                                      int clockRate,
                                                      int64_t duration,
                                                      bool keyFrame)
{
    if (bytes.empty() || clockRate <= 0) {
        return ::media::Result<MediaRtpAccessUnit>::failure(
            ::media::ErrorInfo::invalidArgument("RTP access unit requires bytes and clock rate"));
    }
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet || av_new_packet(packet.get(), static_cast<int>(bytes.size())) < 0) {
        return ::media::Result<MediaRtpAccessUnit>::failure(
            ::media::ErrorInfo::allocationFailed("RTP access unit AVPacket allocation failed"));
    }
    std::memcpy(packet->data, bytes.data(), bytes.size());
    packet->pts = rtpTimestamp;
    packet->dts = rtpTimestamp;
    packet->duration = duration;
    if (keyFrame) packet->flags |= AV_PKT_FLAG_KEY;
    MediaRtpAccessUnit result;
    result.packet = std::move(packet);
    result.rtpTimestamp = rtpTimestamp;
    result.timeBase = MediaRational{1, clockRate};
    return ::media::Result<MediaRtpAccessUnit>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
