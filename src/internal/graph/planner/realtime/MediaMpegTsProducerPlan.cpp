#include "internal/graph/planner/realtime/MediaMpegTsProducerPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Status validateMpegTsProducerPlan(MediaDatagramProtocolLayout layout,
    const std::optional<MediaMpegTsProducerPlan>& producer)
{
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return ::media::Status::failure(topology.error());
    const bool rtp = layout == MediaDatagramProtocolLayout::MpegTsRtp;
    const bool mpegts = rtp || layout == MediaDatagramProtocolLayout::MpegTsUdp;
    if (mpegts != producer.has_value()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("MPEG-TS producer presence differs from protocol layout"));
    if (!producer) return ::media::Status::success();
    const auto& p = *producer;
    if (p.seriesStorage && !p.muxQueueStorage) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("TS series storage requires its final mux queue product"));
    if (p.muxQueueStorage) {
        auto valid = MediaTsMuxQueueStoragePlanner::validate(*p.muxQueueStorage);
        if (!valid) return valid;
    }
    if (p.seriesStorage) {
        auto valid = MediaTsSeriesStoragePlanner::validate(*p.seriesStorage);
        if (!valid) return valid;
    }
    const std::uint64_t header = rtp ? 12 : 0;
    if (p.maximumDatagramBytes <= header ||
        p.maximumDatagramBytes > (std::numeric_limits<std::size_t>::max)() ||
        (p.maximumDatagramBytes - header) % 188 ||
        (rtp ? (!p.maximumRtcpCompoundBytes ||
            p.maximumRtcpCompoundBytes > p.maximumDatagramBytes ||
            MediaRtcpWireGeometry::byePayloadBytes() > p.maximumDatagramBytes - p.maximumRtcpCompoundBytes)
             : p.maximumRtcpCompoundBytes != 0))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS producer requires complete TS datagrams and bounded RTCP geometry"));
    return ::media::Status::success();
}

::media::Result<std::optional<MediaMpegTsProducerPlan>> planMpegTsProducer(
    MediaDatagramProtocolLayout layout, std::size_t maximumUdpPayloadBytes, std::string_view cname)
{
    using Result = ::media::Result<std::optional<MediaMpegTsProducerPlan>>;
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return Result::failure(topology.error());
    const bool rtp = layout == MediaDatagramProtocolLayout::MpegTsRtp;
    if (!rtp && layout != MediaDatagramProtocolLayout::MpegTsUdp) return Result::success(std::nullopt);
    auto packets = MediaTsMuxPlan::maximumPacketsPerDatagram(maximumUdpPayloadBytes,
        rtp ? MediaOutputTransportKind::RtpAvp : MediaOutputTransportKind::UdpDatagrams);
    if (!packets) return Result::failure(packets.error());
    std::uint64_t rtcp = 0;
    if (rtp) {
        auto compound = MediaRtcpWireGeometry::compoundPayloadBytes(cname.size());
        if (!compound) return Result::failure(compound.error());
        rtcp = compound.value();
    }
    MediaMpegTsProducerPlan result{static_cast<std::uint64_t>(packets.value()) * 188 + (rtp ? 12 : 0), rtcp};
    auto valid = validateMpegTsProducerPlan(layout, result);
    return valid ? Result::success(result) : Result::failure(valid.error());
}
} // namespace media::ffmpeg::graph
