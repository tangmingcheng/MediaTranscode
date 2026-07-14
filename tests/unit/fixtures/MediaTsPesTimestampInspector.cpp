#include "MediaTsPesTimestampInspector.h"

namespace media_transcode::test {
namespace {

::media::Result<std::uint64_t> timestamp(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::uint8_t expectedPrefix)
{
    if (bytes.size() < offset + 5 ||
        (bytes[offset] >> 4) != expectedPrefix ||
        (bytes[offset] & 1) == 0 || (bytes[offset + 2] & 1) == 0 ||
        (bytes[offset + 4] & 1) == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument("test PES timestamp is malformed"));
    }
    const std::uint64_t value =
        (static_cast<std::uint64_t>((bytes[offset] >> 1) & 7) << 30) |
        (static_cast<std::uint64_t>(bytes[offset + 1]) << 22) |
        (static_cast<std::uint64_t>((bytes[offset + 2] >> 1) & 0x7F) << 15) |
        (static_cast<std::uint64_t>(bytes[offset + 3]) << 7) |
        static_cast<std::uint64_t>((bytes[offset + 4] >> 1) & 0x7F);
    return ::media::Result<std::uint64_t>::success(value);
}

} // namespace

::media::Status MediaTsPesTimestampInspector::onPacket(
    const media::ffmpeg::graph::MediaTsPacketView& packet)
{
    if (packet.pcr27Mhz) m_pcrValues.push_back(*packet.pcr27Mhz);
    const auto bytes = packet.payloadSpan;
    if (!packet.payloadUnitStart || bytes.size() < 14 || bytes[0] != 0 ||
        bytes[1] != 0 || bytes[2] != 1) {
        return ::media::Status::success();
    }
    const std::uint8_t flags = bytes[7] & 0xC0;
    if (flags != 0x80 && flags != 0xC0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("test PES timestamp flags are invalid"));
    }
    auto pts = timestamp(bytes, 9, flags == 0xC0 ? 3 : 2);
    if (!pts) return ::media::Status::failure(pts.error());
    std::uint64_t dts = pts.value();
    if (flags == 0xC0) {
        auto parsedDts = timestamp(bytes, 14, 1);
        if (!parsedDts) return ::media::Status::failure(parsedDts.error());
        dts = parsedDts.value();
    }
    m_timestamps.push_back(MediaTsObservedPesTimestamp{
        packet.pid, pts.value(), dts});
    return ::media::Status::success();
}

::media::Status MediaTsPesTimestampInspector::onContinuityEvent(
    const media::ffmpeg::graph::MediaTsContinuityEvent&)
{
    ++m_continuityEvents;
    return ::media::Status::success();
}

} // namespace media_transcode::test
