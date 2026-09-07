#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <cstring>

namespace media::ffmpeg::graph {
namespace {
constexpr std::uint8_t StartCode[] = {0, 0, 0, 1};
}

::media::Result<std::size_t> rtpAccessUnitNalCapacity(
    std::size_t assembledBytes, std::size_t maximumAccessUnitBytes)
{
    if (assembledBytes > maximumAccessUnitBytes ||
        maximumAccessUnitBytes - assembledBytes <= sizeof(StartCode)) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP access unit exceeds its prepared byte capacity"));
    }
    return ::media::Result<std::size_t>::success(
        maximumAccessUnitBytes - assembledBytes - sizeof(StartCode));
}

::media::Status appendRtpAccessUnitNal(
    std::vector<std::uint8_t>& output, std::span<const std::uint8_t> nal,
    std::size_t maximumAccessUnitBytes)
{
    auto capacity = rtpAccessUnitNalCapacity(output.size(), maximumAccessUnitBytes);
    if (!capacity) return ::media::Status::failure(capacity.error());
    if (nal.size() > capacity.value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP NAL exceeds the remaining prepared access unit capacity"));
    }
    output.insert(output.end(), std::begin(StartCode), std::end(StartCode));
    output.insert(output.end(), nal.begin(), nal.end());
    return ::media::Status::success();
}

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
