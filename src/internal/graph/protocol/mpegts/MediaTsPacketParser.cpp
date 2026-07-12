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

    if (bufferedSize() == 0) {
        m_buffer.clear();
        m_bufferBegin = 0;
        m_bufferOffset = m_nextInputOffset;
    }
    m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
    m_nextInputOffset += bytes.size();
    return processBufferedBytes();
}

std::size_t MediaTsPacketParser::bufferedSize() const noexcept
{
    return m_buffer.size() - m_bufferBegin;
}

void MediaTsPacketParser::consumeBufferedBytes(std::size_t count)
{
    m_bufferBegin += count;
    m_bufferOffset += count;
    if (m_bufferBegin >= 4096 && m_bufferBegin * 2 >= m_buffer.size()) {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_bufferBegin);
        m_bufferBegin = 0;
    }
}

::media::Status MediaTsPacketParser::processBufferedBytes()
{
    constexpr std::size_t PacketSize = 188;
    while (true) {
        if (!m_strideLocked) {
            bool acquired = false;
            for (std::size_t index = m_bufferBegin;
                 index + PacketSize < m_buffer.size(); ++index) {
                if (m_buffer[index] == 0x47 && m_buffer[index + PacketSize] == 0x47) {
                    consumeBufferedBytes(index - m_bufferBegin);
                    m_strideLocked = true;
                    acquired = true;
                    break;
                }
            }
            if (!acquired) {
                if (bufferedSize() > PacketSize) consumeBufferedBytes(bufferedSize() - PacketSize);
                return ::media::Status::success();
            }
        }

        if (bufferedSize() < PacketSize) return ::media::Status::success();
        if (m_buffer[m_bufferBegin] != 0x47) {
            m_strideLocked = false;
            consumeBufferedBytes(1);
            continue;
        }
        const auto packet = std::span<const uint8_t>(m_buffer).subspan(m_bufferBegin, PacketSize);
        auto status = parsePacket(packet, m_bufferOffset);
        if (!status) {
            consumeBufferedBytes(PacketSize);
            return status;
        }
        consumeBufferedBytes(PacketSize);
    }
}

::media::Status MediaTsPacketParser::parsePacket(std::span<const uint8_t> packet,
                                                 uint64_t byteOffset)
{
    if (packet[0] != 0x47) return invalidPacket("MPEG-TS packet has invalid sync byte");
    if ((packet[1] & 0x80) != 0) return invalidPacket("MPEG-TS transport error indicator is set");
    if ((packet[3] & 0xC0) != 0) return invalidPacket("scrambled MPEG-TS packets are unsupported");

    const uint16_t pid = static_cast<uint16_t>(((packet[1] & 0x1F) << 8) | packet[2]);
    const bool payloadUnitStart = (packet[1] & 0x40) != 0;
    const uint8_t adaptationControl = static_cast<uint8_t>((packet[3] >> 4) & 0x03);
    const uint8_t continuityCounter = static_cast<uint8_t>(packet[3] & 0x0F);
    if (adaptationControl == 0) return invalidPacket("MPEG-TS adaptation field control is reserved");

    const bool hasAdaptation = (adaptationControl & 0x02) != 0;
    const bool hasPayload = (adaptationControl & 0x01) != 0;
    std::size_t payloadOffset = 4;
    bool discontinuity = false;
    std::optional<uint64_t> pcr;
    if (hasAdaptation) {
        const std::size_t adaptationLength = packet[4];
        if (adaptationLength > 183 || 5 + adaptationLength > packet.size()) {
            return invalidPacket("MPEG-TS adaptation field length exceeds packet boundary");
        }
        if (!hasPayload && adaptationLength != 183) {
            return invalidPacket("adaptation-only MPEG-TS packet does not consume packet body");
        }
        payloadOffset = 5 + adaptationLength;
        if (hasPayload && payloadOffset >= packet.size()) {
            return invalidPacket("MPEG-TS packet declares payload without payload bytes");
        }
        if (adaptationLength != 0) {
            const uint8_t flags = packet[5];
            discontinuity = (flags & 0x80) != 0;
            std::size_t cursor = 6;
            const std::size_t adaptationEnd = 5 + adaptationLength;
            auto require = [&](std::size_t count, const char* message) -> ::media::Status {
                return count <= adaptationEnd - cursor
                    ? ::media::Status::success()
                    : invalidPacket(message);
            };
            auto parseClock = [&](bool publish) -> ::media::Status {
                auto enough = require(6, "MPEG-TS adaptation clock field is truncated");
                if (!enough) return enough;
                if ((packet[cursor + 4] & 0x7E) != 0x7E) {
                    return invalidPacket("MPEG-TS adaptation clock reserved bits are invalid");
                }
                const uint64_t base = (static_cast<uint64_t>(packet[cursor]) << 25) |
                                      (static_cast<uint64_t>(packet[cursor + 1]) << 17) |
                                      (static_cast<uint64_t>(packet[cursor + 2]) << 9) |
                                      (static_cast<uint64_t>(packet[cursor + 3]) << 1) |
                                      (static_cast<uint64_t>(packet[cursor + 4]) >> 7);
                const uint16_t extension = static_cast<uint16_t>(((packet[cursor + 4] & 1) << 8) |
                                                                  packet[cursor + 5]);
                if (extension > 299) return invalidPacket("MPEG-TS adaptation clock extension exceeds 299");
                if (publish) pcr = base * 300 + extension;
                cursor += 6;
                return ::media::Status::success();
            };
            if ((flags & 0x10) != 0) { auto status = parseClock(true); if (!status) return status; }
            if ((flags & 0x08) != 0) { auto status = parseClock(false); if (!status) return status; }
            if ((flags & 0x04) != 0) {
                auto status = require(1, "MPEG-TS splice countdown is truncated");
                if (!status) return status;
                ++cursor;
            }
            if ((flags & 0x02) != 0) {
                auto status = require(1, "MPEG-TS private data length is truncated");
                if (!status) return status;
                const std::size_t length = packet[cursor++];
                status = require(length, "MPEG-TS private data exceeds adaptation field");
                if (!status) return status;
                cursor += length;
            }
            if ((flags & 0x01) != 0) {
                auto status = require(1, "MPEG-TS adaptation extension length is truncated");
                if (!status) return status;
                const std::size_t extensionLength = packet[cursor++];
                status = require(extensionLength, "MPEG-TS adaptation extension exceeds adaptation field");
                if (!status) return status;
                const std::size_t extensionEnd = cursor + extensionLength;
                if (extensionLength != 0) {
                    const uint8_t extensionFlags = packet[cursor++];
                    auto requireExtension = [&](std::size_t count, const char* message) -> ::media::Status {
                        return count <= extensionEnd - cursor
                            ? ::media::Status::success()
                            : invalidPacket(message);
                    };
                    if ((extensionFlags & 0x80) != 0) {
                        status = requireExtension(2, "MPEG-TS LTW field is truncated");
                        if (!status) return status;
                        cursor += 2;
                    }
                    if ((extensionFlags & 0x40) != 0) {
                        status = requireExtension(3, "MPEG-TS piecewise rate field is truncated");
                        if (!status) return status;
                        cursor += 3;
                    }
                    if ((extensionFlags & 0x20) != 0) {
                        status = requireExtension(5, "MPEG-TS seamless splice field is truncated");
                        if (!status) return status;
                        cursor += 5;
                    }
                }
                cursor = extensionEnd;
            }
            for (; cursor < adaptationEnd; ++cursor) {
                if (packet[cursor] != 0xFF) return invalidPacket("MPEG-TS adaptation stuffing byte is invalid");
            }
        }
    }

    const auto previous = m_continuity.find(pid);
    if (!discontinuity && previous != m_continuity.end()) {
        const uint8_t expected = hasPayload
            ? static_cast<uint8_t>((previous->second.counter + 1) & 0x0F)
            : previous->second.counter;
        if (continuityCounter != expected) {
            auto resetStatus = m_sink.onContinuityLoss(pid);
            m_continuity.erase(pid);
            if (!resetStatus) return resetStatus;
            return invalidPacket("MPEG-TS continuity counter loss");
        }
    }

    const std::span<const uint8_t> payload = hasPayload
        ? packet.subspan(payloadOffset)
        : std::span<const uint8_t>{};
    const MediaTsPacketView view{
        byteOffset, pid, payloadUnitStart, continuityCounter, discontinuity, pcr, payload};
    auto status = m_sink.onPacket(view);
    if (!status) return status;
    m_continuity[pid] = ContinuityState{continuityCounter};
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
