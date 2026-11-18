#include "internal/graph/nodes/mux/ScheduledDeterministicVideoRtpPacketizerSession.h"

#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <new>
#include <utility>

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
    const auto bytes = m_config.emissionContract().maximumDatagramBytes();
    if (bytes <= RtpHeaderBytes ||
        bytes != static_cast<std::size_t>(m_config.avioConfig().maximumDatagramBytes())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "video RTP workspace differs from its emission contract"));
    }
    m_open = true;
    return ::media::Status::success();
}

::media::Status
ScheduledDeterministicVideoRtpPacketizerSession::writeAccessUnit(
    const AVPacket& packet,
    MediaRtpTimestamp timestamp, std::span<std::uint8_t> scratch)
{
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    const auto& contract = m_config.emissionContract();
    if (!m_open || scratch.size() != contract.maximumDatagramBytes() || !packet.data || packet.size <= 0 ||
        static_cast<std::uint64_t>(packet.size) >
            contract.maximumAccessUnitPayloadBytes()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "deterministic video RTP access unit exceeds its contract"));
    }
    const auto mode = m_config.packetizationMode();
    const auto codec = mode == MediaScheduledRtpPacketizationMode::H264AnnexB
        ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
    struct Emission final {
        ScheduledDeterministicVideoRtpPacketizerSession& session;
        MediaRtpTimestamp timestamp;
        std::span<std::uint8_t> scratch;
        std::uint64_t count;
    } emission{*this, timestamp, scratch, 0};
    try {
        auto packetized = MediaDeterministicVideoRtpPacketizer::packetize(
            std::span<const std::uint8_t>(
                packet.data, static_cast<std::size_t>(packet.size)),
            codec, *contract.packetLayout(),
            scratch.subspan(RtpHeaderBytes),
            &emission,
            [](void* context, std::span<const std::uint8_t> payload, bool marker) {
                auto& current = *static_cast<Emission*>(context);
                auto& session = current.session;
                if (current.count >= session.m_config.emissionContract().maximumDatagramsPerAccessUnit()) {
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "deterministic video RTP datagram count exceeds its contract"));
                }
                auto* datagram = current.scratch.data();
                datagram[0] = 0x80U;
                datagram[1] = static_cast<std::uint8_t>(
                    session.m_config.identity().payloadType() | (marker ? 0x80U : 0U));
                datagram[2] = 0;
                datagram[3] = 0;
                write32(datagram + 4U, current.timestamp.wire());
                write32(datagram + 8U, session.m_config.identity().ssrc());
                auto emitted = session.m_sink(
                    std::span<const std::uint8_t>(datagram, RtpHeaderBytes + payload.size()),
                    payload.size());
                if (emitted) ++current.count;
                return emitted;
            });
        if (!packetized) {
            m_terminalFailure = packetized.error();
            return packetized;
        }
    } catch (const std::bad_alloc&) {
        const auto error = ::media::ErrorInfo::allocationFailed(
            "deterministic video RTP packetization");
        m_terminalFailure = error;
        return ::media::Status::failure(error);
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
