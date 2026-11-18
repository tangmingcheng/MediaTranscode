#pragma once
#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include <variant>

namespace media::ffmpeg::graph {
class MediaRtpWirePacketPlan final {
public:
    std::size_t bytes() const noexcept
    {
        return std::visit([](const auto& plan) { return plan.bytes(); }, m_payload);
    }
private:
    friend class MediaRtpWirePacketComposer;
    MediaRtpWirePacketPlan(MediaRtpDatagramRewritePlan rewrite,
        MediaRtpTimestamp timestamp, std::uint16_t sequence) noexcept
        : m_payload(rewrite), m_timestamp(timestamp), m_sequence(sequence) {}
    explicit MediaRtpWirePacketPlan(MediaMpegTsRtpPacketPlan plan) noexcept
        : m_payload(plan), m_timestamp(plan.timestamp()), m_sequence(plan.sequenceNumber()) {}
    std::variant<MediaRtpDatagramRewritePlan, MediaMpegTsRtpPacketPlan> m_payload;
    MediaRtpTimestamp m_timestamp;
    std::uint16_t m_sequence;
};
class MediaRtpWirePacketComposer final {
public:
    static ::media::Result<MediaRtpWirePacketPlan> prepare(
        std::span<const std::uint8_t> packetizedRtp, std::size_t expectedPayloadOctets,
        const MediaRtpDatagramRewriteIdentity& identity, std::size_t maximumDatagramBytes,
        MediaRtpTimestamp timestamp, std::uint16_t sequenceNumber);
    static ::media::Result<MediaRtpWirePacketPlan> prepare(
        MediaMpegTsRtpPacketPlan plan,
        const MediaRtpDatagramRewriteIdentity& identity, std::size_t maximumDatagramBytes,
        MediaRtpTimestamp timestamp, std::uint16_t sequenceNumber);
    static ::media::Status writeInto(const MediaRtpWirePacketPlan& plan,
        std::span<std::uint8_t> output);
private:
    MediaRtpWirePacketComposer() = delete;
};
} // namespace media::ffmpeg::graph
