#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"

#include <algorithm>
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

    constexpr std::size_t PacketSize = 188;
    const uint64_t inputOffset = m_nextInputOffset;
    m_nextInputOffset += bytes.size();
    std::size_t inputIndex = 0;

    auto logicalByte = [&](std::size_t index) -> uint8_t {
        return index < m_carrySize
            ? m_carry[index]
            : bytes[inputIndex + index - m_carrySize];
    };
    auto discardLogicalPrefix = [&](std::size_t count) {
        if (count < m_carrySize) {
            std::move(m_carry.begin() + count, m_carry.begin() + m_carrySize, m_carry.begin());
            m_carrySize -= count;
            m_carryOffset += count;
            return;
        }
        inputIndex += count - m_carrySize;
        m_carrySize = 0;
    };
    auto retainLogicalTail = [&]() {
        const std::size_t total = m_carrySize + bytes.size() - inputIndex;
        const std::size_t keep = std::min(PacketSize, total);
        std::array<uint8_t, PacketSize> retained{};
        for (std::size_t index = 0; index < keep; ++index) {
            retained[index] = logicalByte(total - keep + index);
        }
        std::copy_n(retained.begin(), keep, m_carry.begin());
        m_carrySize = keep;
        m_carryOffset = inputOffset + bytes.size() - keep;
        inputIndex = bytes.size();
    };

    while (true) {
        if (m_carrySize != 0) {
            const std::size_t total = m_carrySize + bytes.size() - inputIndex;
            if (m_strideLocked) {
                if (total < PacketSize + 1) {
                    const std::size_t append = bytes.size() - inputIndex;
                    std::copy_n(bytes.begin() + inputIndex, append, m_carry.begin() + m_carrySize);
                    m_carrySize += append;
                    return ::media::Status::success();
                }
                if (m_carry[0] != 0x47 || logicalByte(PacketSize) != 0x47) {
                    m_strideLocked = false;
                    discardLogicalPrefix(1);
                    continue;
                }
                for (std::size_t index = 0; index < PacketSize; ++index) {
                    m_packetScratch[index] = logicalByte(index);
                }
                m_copiedPacketBytes += PacketSize;
                const uint64_t packetOffset = m_carryOffset;
                auto status = parsePacket(m_packetScratch, packetOffset);
                discardLogicalPrefix(PacketSize);
                if (!status) return status;
                continue;
            }

            bool acquired = false;
            for (std::size_t index = 0; index + PacketSize < total; ++index) {
                if (logicalByte(index) == 0x47 && logicalByte(index + PacketSize) == 0x47) {
                    discardLogicalPrefix(index);
                    m_strideLocked = true;
                    acquired = true;
                    break;
                }
            }
            if (!acquired) {
                retainLogicalTail();
                return ::media::Status::success();
            }
            continue;
        }

        const auto remaining = bytes.subspan(inputIndex);
        if (m_strideLocked) {
            if (remaining.size() < PacketSize + 1) {
                std::copy(remaining.begin(), remaining.end(), m_carry.begin());
                m_carrySize = remaining.size();
                m_carryOffset = inputOffset + inputIndex;
                return ::media::Status::success();
            }
            if (remaining[0] != 0x47 || remaining[PacketSize] != 0x47) {
                m_strideLocked = false;
                ++inputIndex;
                continue;
            }
            auto status = parsePacket(remaining.first(PacketSize), inputOffset + inputIndex);
            inputIndex += PacketSize;
            if (!status) return status;
            continue;
        }

        bool acquired = false;
        for (std::size_t index = 0; index + PacketSize < remaining.size(); ++index) {
            if (remaining[index] == 0x47 && remaining[index + PacketSize] == 0x47) {
                inputIndex += index;
                m_strideLocked = true;
                acquired = true;
                break;
            }
        }
        if (!acquired) {
            retainLogicalTail();
            return ::media::Status::success();
        }
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
