#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t PacketSize = 188;
constexpr std::uint64_t PcrModulus = (std::uint64_t{1} << 33) * 300;
constexpr std::uint16_t NullPid = 0x1FFF;

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

class SegmentedLogicalBytes final {
public:
    static ::media::Result<SegmentedLogicalBytes> create(
        std::span<const std::span<const std::uint8_t>> segments)
    {
        std::size_t total = 0;
        for (const auto segment : segments) {
            if (segment.size() > std::numeric_limits<std::size_t>::max() - total) {
                return ::media::Result<SegmentedLogicalBytes>::failure(
                    invalid("MPEG-TS logical unit byte count overflows"));
            }
            total += segment.size();
        }
        if (total == 0) {
            return ::media::Result<SegmentedLogicalBytes>::failure(
                invalid("MPEG-TS logical unit is empty"));
        }
        return ::media::Result<SegmentedLogicalBytes>::success(
            SegmentedLogicalBytes(segments, total));
    }

    std::size_t size() const noexcept { return m_total; }

    void copyNext(std::span<std::uint8_t> destination) noexcept
    {
        std::size_t written = 0;
        while (written < destination.size()) {
            const auto segment = m_segments[m_segmentIndex];
            const std::size_t available = segment.size() - m_segmentOffset;
            const std::size_t count = std::min(available, destination.size() - written);
            std::copy_n(segment.begin() + m_segmentOffset, count,
                        destination.begin() + written);
            written += count;
            m_segmentOffset += count;
            if (m_segmentOffset == segment.size()) {
                ++m_segmentIndex;
                m_segmentOffset = 0;
            }
        }
    }

private:
    SegmentedLogicalBytes(std::span<const std::span<const std::uint8_t>> segments,
                          std::size_t total) noexcept
        : m_segments(segments), m_total(total) {}

    std::span<const std::span<const std::uint8_t>> m_segments;
    std::size_t m_total;
    std::size_t m_segmentIndex = 0;
    std::size_t m_segmentOffset = 0;
};

std::uint8_t previousPayload(std::uint8_t next) noexcept
{
    return static_cast<std::uint8_t>((next + 15) & 0x0F);
}

void writeHeader(std::array<std::uint8_t, PacketSize>& packet,
                 std::uint16_t pid,
                 bool payloadUnitStart,
                 std::uint8_t adaptationControl,
                 std::uint8_t continuity) noexcept
{
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<std::uint8_t>((payloadUnitStart ? 0x40 : 0) |
                                          ((pid >> 8) & 0x1F));
    packet[2] = static_cast<std::uint8_t>(pid);
    packet[3] = static_cast<std::uint8_t>((adaptationControl << 4) |
                                          (continuity & 0x0F));
}

} // namespace

::media::Result<std::vector<std::array<std::uint8_t, 188>>>
MediaTsTransportPacketBuilder::payload(
    std::uint16_t pid,
    std::uint8_t initialPayloadContinuity,
    std::span<const std::span<const std::uint8_t>> segments,
    bool randomAccess,
    bool discontinuity,
    std::vector<std::array<std::uint8_t, 188>> workspace)
{
    if (pid >= NullPid || initialPayloadContinuity > 15) {
        return ::media::Result<std::vector<std::array<std::uint8_t, 188>>>::failure(
            invalid("MPEG-TS packet builder identity is invalid"));
    }
    auto logicalBytes = SegmentedLogicalBytes::create(segments);
    if (!logicalBytes) {
        return ::media::Result<std::vector<std::array<std::uint8_t, 188>>>::failure(
            logicalBytes.error());
    }
    auto reader = std::move(logicalBytes).value();
    workspace.clear();
    const std::size_t firstCapacity =
        randomAccess || discontinuity ? 182 : 184;
    const std::size_t remaining = reader.size() > firstCapacity
        ? reader.size() - firstCapacity
        : 0;
    const std::size_t trailingPackets = remaining / 184 + (remaining % 184 != 0);
    workspace.reserve(1 + trailingPackets);
    std::size_t offset = 0;
    std::uint8_t nextContinuity = initialPayloadContinuity;
    while (offset < reader.size()) {
        const bool first = offset == 0;
        const bool needsRandomAccess = first && randomAccess;
        const bool needsDiscontinuity = first && discontinuity;
        const std::size_t remaining = reader.size() - offset;
        const std::size_t payloadBytes =
            needsRandomAccess || needsDiscontinuity
            ? std::min<std::size_t>(remaining, 182)
            : std::min<std::size_t>(remaining, 184);
        const bool needsAdaptation =
            needsRandomAccess || needsDiscontinuity || payloadBytes < 184;

        std::array<std::uint8_t, PacketSize> packet;
        writeHeader(packet, pid, first,
                    static_cast<std::uint8_t>(needsAdaptation ? 3 : 1),
                    nextContinuity);
        std::size_t payloadOffset = 4;
        if (needsAdaptation) {
            const std::size_t adaptationLength = 183 - payloadBytes;
            packet[4] = static_cast<std::uint8_t>(adaptationLength);
            if (adaptationLength != 0) {
                packet[5] = static_cast<std::uint8_t>(
                    (needsRandomAccess ? 0x40 : 0) |
                    (needsDiscontinuity ? 0x80 : 0));
            }
            payloadOffset = 5 + adaptationLength;
        }
        reader.copyNext(std::span<std::uint8_t>(packet).subspan(
            payloadOffset, payloadBytes));
        offset += payloadBytes;
        nextContinuity = static_cast<std::uint8_t>((nextContinuity + 1) & 0x0F);
        workspace.push_back(std::move(packet));
    }
    return ::media::Result<std::vector<std::array<std::uint8_t, 188>>>::success(
        std::move(workspace));
}

::media::Result<std::array<std::uint8_t, 188>>
MediaTsTransportPacketBuilder::pcrOnly(
    std::uint16_t pid,
    std::uint8_t nextPayloadContinuity,
    std::uint64_t wire27Mhz,
    bool discontinuity)
{
    if (pid >= NullPid || nextPayloadContinuity > 15 ||
        wire27Mhz >= PcrModulus) {
        return ::media::Result<std::array<std::uint8_t, 188>>::failure(
            invalid("MPEG-TS PCR wire value is outside its 42-bit field"));
    }
    std::array<std::uint8_t, PacketSize> packet;
    writeHeader(packet, pid, false, 2, previousPayload(nextPayloadContinuity));
    packet[4] = 183;
    packet[5] = static_cast<std::uint8_t>(
        0x10 | (discontinuity ? 0x80 : 0));
    const std::uint64_t base = wire27Mhz / 300;
    const std::uint16_t extension = static_cast<std::uint16_t>(wire27Mhz % 300);
    packet[6] = static_cast<std::uint8_t>(base >> 25);
    packet[7] = static_cast<std::uint8_t>(base >> 17);
    packet[8] = static_cast<std::uint8_t>(base >> 9);
    packet[9] = static_cast<std::uint8_t>(base >> 1);
    packet[10] = static_cast<std::uint8_t>((base << 7) | 0x7E |
                                           (extension >> 8));
    packet[11] = static_cast<std::uint8_t>(extension);
    return ::media::Result<std::array<std::uint8_t, 188>>::success(
        std::move(packet));
}

} // namespace media::ffmpeg::graph
