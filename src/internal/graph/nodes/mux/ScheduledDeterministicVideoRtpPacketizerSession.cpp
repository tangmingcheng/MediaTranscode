#include "internal/graph/nodes/mux/ScheduledDeterministicVideoRtpPacketizerSession.h"

#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpHeaderBytes = 12;

void write32(std::uint8_t* output, std::uint32_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value >> 24U);
    output[1] = static_cast<std::uint8_t>(value >> 16U);
    output[2] = static_cast<std::uint8_t>(value >> 8U);
    output[3] = static_cast<std::uint8_t>(value);
}

} // namespace

ScheduledDeterministicVideoRtpPacketizerSession::
ScheduledDeterministicVideoRtpPacketizerSession(
    ScheduledRtpMuxStreamConfig config,
    ScheduledRtpRewrittenDatagramSink sink)
    : m_config(std::move(config)), m_sink(std::move(sink))
{
}

::media::Status ScheduledDeterministicVideoRtpPacketizerSession::open()
{
    if (m_open || m_terminalFailure || !m_sink ||
        !m_config.emissionContract().packetLayout()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "deterministic video RTP session cannot open"));
    }
    m_open = true;
    return ::media::Status::success();
}

::media::Status
ScheduledDeterministicVideoRtpPacketizerSession::writeAccessUnit(
    const AVPacket& packet,
    MediaRtpTimestamp timestamp)
{
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    const auto& contract = m_config.emissionContract();
    if (!m_open || !packet.data || packet.size <= 0 ||
        static_cast<std::uint64_t>(packet.size) >
            contract.maximumAccessUnitPayloadBytes()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "deterministic video RTP access unit exceeds its contract"));
    }
    const auto mode = m_config.packetizationMode();
    const auto codec = mode == MediaScheduledRtpPacketizationMode::H264AnnexB
        ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
    const auto maximumDatagram = static_cast<std::size_t>(
        m_config.avioConfig().maximumDatagramBytes());
    auto payloads = MediaDeterministicVideoRtpPacketizer::packetize(
        std::span<const std::uint8_t>(
            packet.data, static_cast<std::size_t>(packet.size)),
        codec, *contract.packetLayout(), maximumDatagram - RtpHeaderBytes);
    if (!payloads || payloads.value().size() >
            contract.maximumDatagramsPerAccessUnit()) {
        const auto error = payloads
            ? ::media::ErrorInfo::invalidArgument(
                "deterministic video RTP datagram count exceeds its contract")
            : payloads.error();
        m_terminalFailure = error;
        return ::media::Status::failure(error);
    }
    try {
        std::vector<std::vector<std::uint8_t>> datagrams;
        datagrams.reserve(payloads.value().size());
        for (const auto& packetized : payloads.value()) {
            std::vector<std::uint8_t> datagram(
                RtpHeaderBytes + packetized.payload.size());
            datagram[0] = 0x80U;
            datagram[1] = static_cast<std::uint8_t>(
                m_config.identity().payloadType() |
                (packetized.marker ? 0x80U : 0U));
            write32(datagram.data() + 4U, timestamp.wire());
            write32(datagram.data() + 8U, m_config.identity().ssrc());
            std::copy(packetized.payload.begin(), packetized.payload.end(),
                      datagram.begin() + RtpHeaderBytes);
            datagrams.push_back(std::move(datagram));
        }
        for (const auto& datagram : datagrams) {
            auto emitted = m_sink(
                datagram, datagram.size() - RtpHeaderBytes);
            if (!emitted) {
                m_terminalFailure = emitted.error();
                return emitted;
            }
        }
    } catch (const std::bad_alloc&) {
        const auto error = ::media::ErrorInfo::allocationFailed(
            "deterministic video RTP datagrams");
        m_terminalFailure = error;
        return ::media::Status::failure(error);
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
