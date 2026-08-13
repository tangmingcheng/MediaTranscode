#include "internal/graph/protocol/sdp/MediaHevcSdpCodecDescriptionFactory.h"

#include "internal/graph/protocol/sdp/MediaSdpBase64Encoder.h"

extern "C" {
#include <libavcodec/codec_par.h>
}

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct ParameterSets final {
    std::array<std::vector<std::vector<std::uint8_t>>, 3> byType;
};

std::size_t setIndex(std::uint8_t nalType) noexcept
{
    return static_cast<std::size_t>(nalType - 32u);
}

bool validParameterSet(
    std::span<const std::uint8_t> nal,
    std::uint8_t expectedType) noexcept
{
    return nal.size() >= 2 && (nal[0] & 0x80u) == 0 &&
        ((nal[0] >> 1) & 0x3fu) == expectedType &&
        (nal[1] & 0x07u) != 0;
}

bool readU16(
    std::span<const std::uint8_t> bytes,
    std::size_t& offset,
    std::size_t& value) noexcept
{
    if (offset + 2 > bytes.size()) return false;
    value = (static_cast<std::size_t>(bytes[offset]) << 8) |
        bytes[offset + 1];
    offset += 2;
    return true;
}

std::size_t startCodeSize(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept
{
    if (offset + 4 <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1] == 0 && bytes[offset + 2] == 0 &&
        bytes[offset + 3] == 1) return 4;
    if (offset + 3 <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1] == 0 && bytes[offset + 2] == 1) return 3;
    return 0;
}

::media::Result<ParameterSets> parseAnnexB(
    std::span<const std::uint8_t> bytes)
{
    ParameterSets result;
    std::size_t cursor = 0;
    while (cursor < bytes.size()) {
        const std::size_t marker = startCodeSize(bytes, cursor);
        if (marker == 0 || cursor + marker + 2 > bytes.size()) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported(
                    "Malformed HEVC Annex-B codec configuration"));
        }
        const std::size_t nalBegin = cursor + marker;
        std::size_t next = nalBegin + 2;
        while (next < bytes.size() && startCodeSize(bytes, next) == 0) ++next;
        std::size_t nalEnd = next;
        while (nalEnd > nalBegin && bytes[nalEnd - 1] == 0) --nalEnd;
        const std::span<const std::uint8_t> nal =
            bytes.subspan(nalBegin, nalEnd - nalBegin);
        const std::uint8_t type = (nal[0] >> 1) & 0x3fu;
        if (type >= 32 && type <= 34) {
            if (!validParameterSet(nal, type)) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported(
                        "Invalid HEVC Annex-B parameter set"));
            }
            result.byType[setIndex(type)].emplace_back(
                nal.begin(), nal.end());
        }
        if (next == bytes.size()) break;
        cursor = next;
    }
    return ::media::Result<ParameterSets>::success(std::move(result));
}

::media::Result<ParameterSets> parseHvcc(
    std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 23 || bytes[0] != 1) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported(
                "Invalid HEVCDecoderConfigurationRecord header"));
    }
    ParameterSets result;
    std::size_t offset = 23;
    const std::size_t arrayCount = bytes[22];
    for (std::size_t arrayIndex = 0; arrayIndex < arrayCount; ++arrayIndex) {
        if (offset >= bytes.size()) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported(
                    "Truncated HEVC configuration array"));
        }
        const std::uint8_t type = bytes[offset++] & 0x3fu;
        std::size_t nalCount = 0;
        if (!readU16(bytes, offset, nalCount)) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported(
                    "Truncated HEVC configuration NAL count"));
        }
        for (std::size_t nalIndex = 0; nalIndex < nalCount; ++nalIndex) {
            std::size_t nalSize = 0;
            if (!readU16(bytes, offset, nalSize) || nalSize == 0 ||
                offset + nalSize > bytes.size()) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported(
                        "Invalid HEVC configuration NAL size"));
            }
            const auto nal = bytes.subspan(offset, nalSize);
            if (!validParameterSet(nal, type)) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported(
                        "HEVC configuration array contradicts its NAL type"));
            }
            if (type >= 32 && type <= 34) {
                result.byType[setIndex(type)].emplace_back(
                    nal.begin(), nal.end());
            }
            offset += nalSize;
        }
    }
    if (offset != bytes.size()) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported(
                "Trailing HEVC configuration bytes"));
    }
    return ::media::Result<ParameterSets>::success(std::move(result));
}

::media::Result<std::string> encodeSets(
    const std::vector<std::vector<std::uint8_t>>& sets)
{
    std::string result;
    for (const auto& set : sets) {
        auto encoded = MediaSdpBase64Encoder::encode(set);
        if (!encoded) {
            return ::media::Result<std::string>::failure(encoded.error());
        }
        if (!result.empty()) result.push_back(',');
        result += encoded.value();
    }
    return ::media::Result<std::string>::success(std::move(result));
}

} // namespace

::media::Result<MediaHevcSdpCodecDescription>
MediaHevcSdpCodecDescriptionFactory::create(
    const AVCodecParameters& parameters,
    std::span<const std::uint8_t> accessUnitConfiguration)
{
    if (parameters.codec_type != AVMEDIA_TYPE_VIDEO ||
        parameters.codec_id != AV_CODEC_ID_HEVC) {
        return ::media::Result<MediaHevcSdpCodecDescription>::failure(
            ::media::ErrorInfo::unsupported(
                "Final codec parameters are not complete HEVC"));
    }
    const bool hasExtradata = parameters.extradata &&
        parameters.extradata_size > 0;
    const std::span<const std::uint8_t> bytes = hasExtradata
        ? std::span<const std::uint8_t>(
              parameters.extradata,
              static_cast<std::size_t>(parameters.extradata_size))
        : accessUnitConfiguration;
    if (bytes.empty()) {
        return ::media::Result<MediaHevcSdpCodecDescription>::failure(
            ::media::ErrorInfo::unsupported(
                "HEVC SDP requires codec extradata or a parameter-set access unit"));
    }
    auto sets = bytes[0] == 1 ? parseHvcc(bytes) : parseAnnexB(bytes);
    if (!sets) {
        return ::media::Result<MediaHevcSdpCodecDescription>::failure(
            sets.error());
    }
    for (const auto& typed : sets.value().byType) {
        if (typed.empty()) {
            return ::media::Result<MediaHevcSdpCodecDescription>::failure(
                ::media::ErrorInfo::unsupported(
                    "HEVC SDP requires VPS, SPS, and PPS"));
        }
    }
    auto vps = encodeSets(sets.value().byType[0]);
    auto sps = encodeSets(sets.value().byType[1]);
    auto pps = encodeSets(sets.value().byType[2]);
    if (!vps || !sps || !pps) {
        return ::media::Result<MediaHevcSdpCodecDescription>::failure(
            !vps ? vps.error() : !sps ? sps.error() : pps.error());
    }
    return ::media::Result<MediaHevcSdpCodecDescription>::success(
        MediaHevcSdpCodecDescription(
            std::move(vps).value(), std::move(sps).value(),
            std::move(pps).value()));
}

} // namespace media::ffmpeg::graph
