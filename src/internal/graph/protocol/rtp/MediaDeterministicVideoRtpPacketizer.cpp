#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"

#include <algorithm>
#include <limits>
#include <new>

namespace media::ffmpeg::graph {
namespace {

using PacketResult =
    ::media::Result<std::vector<MediaVideoRtpPayloadPacket>>;

std::size_t headerBytes(MediaAnnexBCodec codec) noexcept
{
    return codec == MediaAnnexBCodec::H264 ? 1U : 2U;
}

std::vector<std::uint8_t> aggregate(
    std::span<const std::span<const std::uint8_t>> units,
    MediaAnnexBCodec codec)
{
    std::size_t bytes = codec == MediaAnnexBCodec::H264 ? 1U : 2U;
    for (const auto unit : units) bytes += 2U + unit.size();
    std::vector<std::uint8_t> payload;
    payload.reserve(bytes);
    if (codec == MediaAnnexBCodec::H264) {
        std::uint8_t nri = 0;
        for (const auto unit : units) {
            nri = (std::max)(
                nri, static_cast<std::uint8_t>(unit[0] & 0x60U));
        }
        payload.push_back(static_cast<std::uint8_t>(nri | 24U));
    } else {
        std::uint8_t minimumLayerId = 0x3fU;
        std::uint8_t minimumTid = 0x07U;
        for (const auto unit : units) {
            const auto layerId = static_cast<std::uint8_t>(
                ((unit[0] & 0x01U) << 5U) | (unit[1] >> 3U));
            minimumLayerId = (std::min)(minimumLayerId, layerId);
            minimumTid = (std::min)(
                minimumTid, static_cast<std::uint8_t>(unit[1] & 0x07U));
        }
        payload.push_back(static_cast<std::uint8_t>(
            (48U << 1U) | (minimumLayerId >> 5U)));
        payload.push_back(static_cast<std::uint8_t>(
            (minimumLayerId << 3U) | minimumTid));
    }
    for (const auto unit : units) {
        payload.push_back(static_cast<std::uint8_t>(unit.size() >> 8U));
        payload.push_back(static_cast<std::uint8_t>(unit.size()));
        payload.insert(payload.end(), unit.begin(), unit.end());
    }
    return payload;
}

void fragment(std::span<const std::uint8_t> nal,
              MediaAnnexBCodec codec,
              std::size_t maximumPayload,
              std::vector<MediaVideoRtpPayloadPacket>& output)
{
    const auto nalHeader = headerBytes(codec);
    const auto fuHeader = codec == MediaAnnexBCodec::H264 ? 2U : 3U;
    const auto capacity = maximumPayload - fuHeader;
    const auto originalType = codec == MediaAnnexBCodec::H264
        ? static_cast<std::uint8_t>(nal[0] & 0x1fU)
        : static_cast<std::uint8_t>((nal[0] >> 1U) & 0x3fU);
    auto remaining = nal.subspan(nalHeader);
    bool first = true;
    while (!remaining.empty()) {
        const auto count = (std::min)(capacity, remaining.size());
        MediaVideoRtpPayloadPacket packet;
        packet.payload.reserve(fuHeader + count);
        if (codec == MediaAnnexBCodec::H264) {
            packet.payload.push_back(static_cast<std::uint8_t>(
                (nal[0] & 0xe0U) | 28U));
        } else {
            packet.payload.push_back(static_cast<std::uint8_t>(
                (nal[0] & 0x81U) | (49U << 1U)));
            packet.payload.push_back(nal[1]);
        }
        std::uint8_t flags = originalType;
        if (first) flags |= 0x80U;
        if (count == remaining.size()) flags |= 0x40U;
        packet.payload.push_back(flags);
        packet.payload.insert(
            packet.payload.end(), remaining.begin(), remaining.begin() + count);
        output.push_back(std::move(packet));
        remaining = remaining.subspan(count);
        first = false;
    }
}

} // namespace

::media::Result<std::uint64_t>
MediaDeterministicVideoRtpPacketizer::maximumDatagramsForPayloadWindow(
    std::uint64_t maximumPayloadBytes,
    std::uint64_t accessUnitCount,
    MediaAnnexBCodec codec,
    std::size_t maximumRtpPayloadBytes)
{
    const std::uint64_t fuHeader =
        codec == MediaAnnexBCodec::H264 ? 2U : 3U;
    if (maximumPayloadBytes == 0 || accessUnitCount == 0 ||
        maximumRtpPayloadBytes <= fuHeader) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "video RTP emission geometry is incomplete"));
    }
    // Greedy AP/STAP packing guarantees that every adjacent pair of output
    // payloads, apart from the terminal payload, consumes at least one FU
    // capacity of access-unit bytes. Prefix bytes already present in Annex-B
    // or length-prefixed input cover each two-byte aggregation length field.
    const auto fragments =
        MediaCheckedArithmetic::ceilScale(
            maximumPayloadBytes, 1U,
            static_cast<std::uint64_t>(maximumRtpPayloadBytes) - fuHeader,
            "deterministic video RTP access-unit fragments");
    if (!fragments) return fragments;
    auto doubled = MediaCheckedArithmetic::multiply(
        fragments.value(), 2U,
        "deterministic video RTP aggregation bound");
    auto accessUnitBound = MediaCheckedArithmetic::multiply(
        accessUnitCount, 3U,
        "deterministic video RTP access-unit boundary bound");
    auto combined = doubled && accessUnitBound
        ? MediaCheckedArithmetic::add(
              doubled.value(), accessUnitBound.value(),
              "deterministic video RTP payload-window bound")
        : (!doubled ? doubled : accessUnitBound);
    if (!combined) return combined;
    return ::media::Result<std::uint64_t>::success(combined.value() - 2U);
}

::media::Result<std::uint64_t>
MediaDeterministicVideoRtpPacketizer::maximumDatagramsPerAccessUnit(
    std::uint64_t maximumAccessUnitBytes,
    MediaAnnexBCodec codec,
    std::size_t maximumRtpPayloadBytes)
{
    return maximumDatagramsForPayloadWindow(
        maximumAccessUnitBytes, 1U, codec, maximumRtpPayloadBytes);
}

PacketResult MediaDeterministicVideoRtpPacketizer::packetize(
    std::span<const std::uint8_t> accessUnit,
    MediaAnnexBCodec codec,
    const MediaEncodedPacketLayout& layout,
    std::size_t maximumRtpPayloadBytes)
{
    const auto fuHeader = codec == MediaAnnexBCodec::H264 ? 2U : 3U;
    if (maximumRtpPayloadBytes <= fuHeader) {
        return PacketResult::failure(::media::ErrorInfo::invalidArgument(
            "video RTP payload bound cannot carry a fragmentation unit"));
    }
    auto parsed = MediaVideoNalUnitScanner::scan(accessUnit, codec, layout);
    if (!parsed) return PacketResult::failure(parsed.error());
    std::vector<MediaVideoRtpPayloadPacket> output;
    try {
        const auto& units = parsed.value();
        for (std::size_t index = 0; index < units.size();) {
            if (units[index].size() > maximumRtpPayloadBytes) {
                fragment(units[index], codec, maximumRtpPayloadBytes, output);
                ++index;
                continue;
            }
            const auto aggregateHeader =
                codec == MediaAnnexBCodec::H264 ? 1U : 2U;
            std::size_t aggregateBytes = aggregateHeader;
            std::size_t end = index;
            while (end < units.size() &&
                   units[end].size() <= maximumRtpPayloadBytes &&
                   units[end].size() <=
                       (std::numeric_limits<std::uint16_t>::max)() &&
                   2U + units[end].size() <=
                       maximumRtpPayloadBytes - aggregateBytes) {
                aggregateBytes += 2U + units[end].size();
                ++end;
            }
            if (end - index >= 2U) {
                output.push_back({aggregate(
                    std::span<const std::span<const std::uint8_t>>(
                        units.data() + index, end - index), codec), false});
                index = end;
            } else {
                output.push_back({
                    std::vector<std::uint8_t>(
                        units[index].begin(), units[index].end()), false});
                ++index;
            }
        }
    } catch (const std::bad_alloc&) {
        return PacketResult::failure(::media::ErrorInfo::allocationFailed(
            "deterministic video RTP payloads"));
    }
    if (output.empty()) {
        return PacketResult::failure(::media::ErrorInfo::invalidArgument(
            "video RTP packetizer emitted no payload"));
    }
    output.back().marker = true;
    return PacketResult::success(std::move(output));
}

} // namespace media::ffmpeg::graph
