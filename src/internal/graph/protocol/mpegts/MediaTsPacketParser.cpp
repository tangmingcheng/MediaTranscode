#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalidPacket(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaTsPacketParser::MediaTsPacketParser(MediaTsPacketSink& sink)
    : m_sink(sink)
{
}

::media::Result<std::unique_ptr<MediaTsPacketParser>> MediaTsPacketParser::create(
    std::size_t packetStride,
    MediaTsPacketSink& sink)
{
    if (packetStride != 188) {
        return ::media::Result<std::unique_ptr<MediaTsPacketParser>>::failure(
            ::media::ErrorInfo::unsupported("MPEG-TS evidence parser supports only 188-byte packets"));
    }
    return ::media::Result<std::unique_ptr<MediaTsPacketParser>>::success(
        std::unique_ptr<MediaTsPacketParser>(new MediaTsPacketParser(sink)));
}

::media::Status MediaTsPacketParser::push(std::span<const uint8_t> bytes)
{
    if (bytes.size() > std::numeric_limits<uint64_t>::max() - m_nextInputOffset) {
        return invalidPacket("MPEG-TS absolute input offset overflow");
    }

    for (const uint8_t byte : bytes) {
        const uint64_t byteOffset = m_nextInputOffset++;
        if (m_retainedSize == 0) {
            if (byte != 0x47) continue;
            m_packetOffset = byteOffset;
        }
        m_packet[m_retainedSize++] = byte;
        if (m_retainedSize != m_packet.size()) continue;

        auto status = parsePacket();
        m_retainedSize = 0;
        if (!status) return status;
    }
    return ::media::Status::success();
}

::media::Status MediaTsPacketParser::parsePacket()
{
    if (m_packet[0] != 0x47) return invalidPacket("MPEG-TS packet has invalid sync byte");
    if ((m_packet[1] & 0x80) != 0) return invalidPacket("MPEG-TS transport error indicator is set");
    if ((m_packet[3] & 0xC0) != 0) return invalidPacket("scrambled MPEG-TS packets are unsupported");

    const uint16_t pid = static_cast<uint16_t>(((m_packet[1] & 0x1F) << 8) | m_packet[2]);
    const bool payloadUnitStart = (m_packet[1] & 0x40) != 0;
    const uint8_t adaptationControl = static_cast<uint8_t>((m_packet[3] >> 4) & 0x03);
    const uint8_t continuityCounter = static_cast<uint8_t>(m_packet[3] & 0x0F);
    if (adaptationControl == 0) return invalidPacket("MPEG-TS adaptation field control is reserved");

    const bool hasAdaptation = (adaptationControl & 0x02) != 0;
    const bool hasPayload = (adaptationControl & 0x01) != 0;
    std::size_t payloadOffset = 4;
    bool discontinuity = false;
    std::optional<uint64_t> pcr;
    if (hasAdaptation) {
        const std::size_t adaptationLength = m_packet[4];
        if (adaptationLength > 183 || 5 + adaptationLength > m_packet.size()) {
            return invalidPacket("MPEG-TS adaptation field length exceeds packet boundary");
        }
        if (!hasPayload && adaptationLength != 183) {
            return invalidPacket("adaptation-only MPEG-TS packet does not consume packet body");
        }
        payloadOffset = 5 + adaptationLength;
        if (hasPayload && payloadOffset >= m_packet.size()) {
            return invalidPacket("MPEG-TS packet declares payload without payload bytes");
        }
        if (adaptationLength != 0) {
            const uint8_t flags = m_packet[5];
            discontinuity = (flags & 0x80) != 0;
            if ((flags & 0x10) != 0) {
                if (adaptationLength < 7) return invalidPacket("MPEG-TS PCR field is truncated");
                if ((m_packet[10] & 0x7E) != 0x7E) {
                    return invalidPacket("MPEG-TS PCR reserved bits are invalid");
                }
                const uint64_t base = (static_cast<uint64_t>(m_packet[6]) << 25) |
                                      (static_cast<uint64_t>(m_packet[7]) << 17) |
                                      (static_cast<uint64_t>(m_packet[8]) << 9) |
                                      (static_cast<uint64_t>(m_packet[9]) << 1) |
                                      (static_cast<uint64_t>(m_packet[10]) >> 7);
                const uint16_t extension = static_cast<uint16_t>(((m_packet[10] & 0x01) << 8) |
                                                                  m_packet[11]);
                if (extension > 299) return invalidPacket("MPEG-TS PCR extension exceeds 299");
                pcr = base * 300 + extension;
            }
        }
    }

    const auto previous = m_continuity.find(pid);
    if (!discontinuity && previous != m_continuity.end()) {
        const uint8_t expected = hasPayload
            ? static_cast<uint8_t>((previous->second.counter + 1) & 0x0F)
            : previous->second.counter;
        if (continuityCounter != expected) {
            return invalidPacket("MPEG-TS continuity counter loss");
        }
    }

    const std::span<const uint8_t> payload = hasPayload
        ? std::span<const uint8_t>(m_packet).subspan(payloadOffset)
        : std::span<const uint8_t>{};
    const MediaTsPacketView view{
        m_packetOffset, pid, payloadUnitStart, continuityCounter, discontinuity, pcr, payload};
    auto status = m_sink.onPacket(view);
    if (!status) return status;
    m_continuity[pid] = ContinuityState{continuityCounter};
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
