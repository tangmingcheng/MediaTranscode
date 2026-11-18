#include "internal/graph/protocol/rtp/MediaRtpWirePacketComposer.h"

namespace media::ffmpeg::graph {
::media::Result<MediaRtpWirePacketPlan> MediaRtpWirePacketComposer::prepare(
    std::span<const std::uint8_t> packetizedRtp, std::size_t expectedPayloadOctets,
    const MediaRtpDatagramRewriteIdentity& identity, std::size_t maximumDatagramBytes,
    MediaRtpTimestamp timestamp, std::uint16_t sequenceNumber)
{
    using Result = ::media::Result<MediaRtpWirePacketPlan>;
    if (expectedPayloadOctets == 0 || packetizedRtp.size() > maximumDatagramBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire packet exceeds its planned datagram or payload bound"));
    auto plan = MediaRtpDatagramRewriter::prepare(packetizedRtp, identity);
    if (!plan) return Result::failure(plan.error());
    if (plan.value().payloadOctets() != expectedPayloadOctets)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP packetizer payload accounting differs from final wire bytes"));
    return Result::success(MediaRtpWirePacketPlan(plan.value(), timestamp, sequenceNumber));
}

::media::Result<MediaRtpWirePacketPlan> MediaRtpWirePacketComposer::prepare(
    MediaMpegTsRtpPacketPlan plan,
    const MediaRtpDatagramRewriteIdentity& identity, std::size_t maximumDatagramBytes,
    MediaRtpTimestamp timestamp, std::uint16_t sequenceNumber)
{
    using Result = ::media::Result<MediaRtpWirePacketPlan>;
    if (plan.bytes() > maximumDatagramBytes ||
        plan.m_payloadType != identity.payloadType() || plan.m_ssrc != identity.ssrc() ||
        plan.timestamp().extendedTicks() != timestamp.extendedTicks()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MP2T packet plan differs from the RTP wire identity, clock, or datagram bound"));
    }
    // Only the session sequence is bound here. The typed packetizer plan already
    // proves complete TS payload and the fixed RTP header without CSRC/extension/padding.
    plan.m_sequence = sequenceNumber;
    return Result::success(MediaRtpWirePacketPlan(plan));
}

::media::Status MediaRtpWirePacketComposer::writeInto(
    const MediaRtpWirePacketPlan& plan, std::span<std::uint8_t> output)
{
    if (const auto* mp2t = std::get_if<MediaMpegTsRtpPacketPlan>(&plan.m_payload))
        return MediaMpegTsRtpPacketizer::writeInto(*mp2t, output);
    auto written = MediaRtpDatagramRewriter::writeInto(
        std::get<MediaRtpDatagramRewritePlan>(plan.m_payload), plan.m_timestamp, output);
    if (!written) return written;
    output[2] = static_cast<std::uint8_t>(plan.m_sequence >> 8);
    output[3] = static_cast<std::uint8_t>(plan.m_sequence);
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
