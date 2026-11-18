#include "internal/graph/planner/realtime/MediaRawRtpProducerPlan.h"

namespace media::ffmpeg::graph {
::media::Status validateRawRtpProducerPlans(MediaDatagramProtocolLayout layout,
    std::span<const MediaRawRtpProducerPlan> producers)
{
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return ::media::Status::failure(topology.error());
    const bool raw = layout == MediaDatagramProtocolLayout::VideoRtp ||
        layout == MediaDatagramProtocolLayout::SeparateAvRtp;
    if (producers.size() != (raw ? topology.value().rtpInstances.size() : 0))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Raw RTP producer set differs from protocol topology"));
    for (std::size_t i = 0; i < producers.size(); ++i) {
        const auto& producer = producers[i];
        const auto mode = producer.emission.packetizationMode();
        const bool video = producer.role == MediaDatagramProtocolEndpointRole::VideoRtp;
        if (producer.role != topology.value().rtpInstances[i].rtp ||
            !producer.maximumRtcpCompoundBytes ||
            producer.maximumRtcpCompoundBytes > producer.emission.maximumDatagramBytes() ||
            producer.emission.maximumDatagramBytes() != producers.front().emission.maximumDatagramBytes() ||
            (video && (!producer.emission.packetLayout() ||
                producer.emission.packetLayout()->kind() != MediaEncodedPacketLayoutKind::StartCodeDelimited)) ||
            (!video && producer.emission.packetLayout()) ||
            (video ? (mode != MediaScheduledRtpPacketizationMode::H264AnnexB &&
                      mode != MediaScheduledRtpPacketizationMode::HevcAnnexB)
                   : mode != MediaScheduledRtpPacketizationMode::AacLatm))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Raw RTP producer facts differ from their protocol role or MTU"));
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
