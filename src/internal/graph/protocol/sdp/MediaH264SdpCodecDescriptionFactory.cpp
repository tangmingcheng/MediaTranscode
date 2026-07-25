#include "internal/graph/protocol/sdp/MediaH264SdpCodecDescriptionFactory.h"

extern "C" {
#include <libavcodec/codec_par.h>
}

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct ParameterSets final {
    std::vector<std::vector<std::uint8_t>> sps;
    std::vector<std::vector<std::uint8_t>> pps;
};

bool readU16(std::span<const std::uint8_t> bytes, std::size_t& offset,
             std::size_t& value) noexcept
{
    if (offset + 2 > bytes.size()) return false;
    value = (static_cast<std::size_t>(bytes[offset]) << 8) | bytes[offset + 1];
    offset += 2;
    return true;
}

bool readNal(std::span<const std::uint8_t> bytes, std::size_t& offset,
             std::vector<std::uint8_t>& nal) noexcept
{
    std::size_t size = 0;
    if (!readU16(bytes, offset, size) || size == 0 || offset + size > bytes.size()) {
        return false;
    }
    nal.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
               bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return true;
}

bool isHighProfileWithExtension(std::uint8_t profile) noexcept
{
    constexpr std::array<std::uint8_t, 13> profiles{
        100, 110, 122, 144, 44, 83, 86, 118, 128, 138, 139, 134, 135};
    for (const auto candidate : profiles) {
        if (candidate == profile) return true;
    }
    return false;
}

::media::Result<ParameterSets> parseAvcc(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 7 || bytes[0] != 1 || (bytes[4] & 0xfc) != 0xfc ||
        (bytes[4] & 0x03) == 2 || (bytes[5] & 0xe0) != 0xe0) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("invalid AVCDecoderConfigurationRecord header"));
    }
    const std::uint8_t headerProfile = bytes[1];
    const std::array<std::uint8_t, 3> headerTriple{bytes[1], bytes[2], bytes[3]};
    std::size_t offset = 6;
    const std::size_t spsCount = bytes[5] & 0x1f;
    if (spsCount == 0) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("AVC configuration has no SPS"));
    }
    ParameterSets result;
    for (std::size_t index = 0; index < spsCount; ++index) {
        std::vector<std::uint8_t> nal;
        if (!readNal(bytes, offset, nal) || nal.size() < 4 || (nal[0] & 0x80) != 0 ||
            (nal[0] & 0x1f) != 7 ||
            !std::equal(headerTriple.begin(), headerTriple.end(), nal.begin() + 1)) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("invalid or inconsistent AVC SPS"));
        }
        result.sps.emplace_back(std::move(nal));
    }
    if (offset >= bytes.size()) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("AVC configuration has no PPS count"));
    }
    const std::size_t ppsCount = bytes[offset++];
    if (ppsCount == 0) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("AVC configuration has no PPS"));
    }
    for (std::size_t index = 0; index < ppsCount; ++index) {
        std::vector<std::uint8_t> nal;
        if (!readNal(bytes, offset, nal) || nal.empty() || (nal[0] & 0x80) != 0 ||
            (nal[0] & 0x1f) != 8) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("invalid AVC PPS"));
        }
        result.pps.emplace_back(std::move(nal));
    }
    if (offset < bytes.size()) {
        if (!isHighProfileWithExtension(headerProfile) || offset + 4 > bytes.size() ||
            (bytes[offset] & 0xfc) != 0xfc ||
            (bytes[offset + 1] & 0xf8) != 0xf8 ||
            (bytes[offset + 2] & 0xf8) != 0xf8) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("invalid AVC high-profile extension"));
        }
        offset += 3;
        const std::size_t extensionCount = bytes[offset++];
        for (std::size_t index = 0; index < extensionCount; ++index) {
            std::vector<std::uint8_t> nal;
            if (!readNal(bytes, offset, nal) || nal.empty() || (nal[0] & 0x80) != 0 ||
                (nal[0] & 0x1f) != 13) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported("invalid AVC SPS extension"));
            }
        }
    }
    if (offset != bytes.size()) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("trailing AVC configuration bytes"));
    }
    return ::media::Result<ParameterSets>::success(std::move(result));
}

std::size_t startCodeSize(std::span<const std::uint8_t> bytes,
                          std::size_t offset) noexcept
{
    if (offset + 3 <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1] == 0 && bytes[offset + 2] == 1) return 3;
    if (offset + 4 <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1] == 0 && bytes[offset + 2] == 0 &&
        bytes[offset + 3] == 1) return 4;
    return 0;
}

::media::Result<ParameterSets> parseAnnexB(std::span<const std::uint8_t> bytes)
{
    std::size_t first = 0;
    while (first < bytes.size() && bytes[first] == 0) ++first;
    if (first == 0 || first >= bytes.size() || bytes[first] != 1 || first < 2) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("H264 extradata is neither AVC configuration nor Annex-B"));
    }
    std::size_t cursor = first - 2;
    ParameterSets result;
    std::array<std::uint8_t, 3> profileTriple{};
    bool haveProfile = false;
    while (cursor < bytes.size()) {
        const std::size_t marker = startCodeSize(bytes, cursor);
        if (marker == 0) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("malformed Annex-B start code"));
        }
        const std::size_t nalBegin = cursor + marker;
        std::size_t next = nalBegin;
        while (next < bytes.size() && startCodeSize(bytes, next) == 0) ++next;
        std::size_t nalEnd = next;
        while (nalEnd > nalBegin && bytes[nalEnd - 1] == 0) --nalEnd;
        if (nalBegin == nalEnd) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("empty Annex-B NAL unit"));
        }
        std::vector<std::uint8_t> nal(
            bytes.begin() + static_cast<std::ptrdiff_t>(nalBegin),
            bytes.begin() + static_cast<std::ptrdiff_t>(nalEnd));
        const std::uint8_t type = nal[0] & 0x1f;
        if ((nal[0] & 0x80) != 0) {
            return ::media::Result<ParameterSets>::failure(
                ::media::ErrorInfo::unsupported("Annex-B NAL forbidden_zero_bit is set"));
        }
        if (type == 7) {
            if (nal.size() < 4) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported("Annex-B SPS is truncated"));
            }
            const std::array<std::uint8_t, 3> candidate{nal[1], nal[2], nal[3]};
            if (haveProfile && candidate != profileTriple) {
                return ::media::Result<ParameterSets>::failure(
                    ::media::ErrorInfo::unsupported("Annex-B SPS profiles disagree"));
            }
            profileTriple = candidate;
            haveProfile = true;
            result.sps.emplace_back(std::move(nal));
        } else if (type == 8) {
            result.pps.emplace_back(std::move(nal));
        }
        if (next == bytes.size()) break;
        cursor = next;
    }
    if (result.sps.empty() || result.pps.empty()) {
        return ::media::Result<ParameterSets>::failure(
            ::media::ErrorInfo::unsupported("Annex-B extradata needs SPS and PPS"));
    }
    return ::media::Result<ParameterSets>::success(std::move(result));
}

std::string base64(std::span<const std::uint8_t> bytes)
{
    constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::uint32_t first = bytes[index];
        const std::uint32_t second = index + 1 < bytes.size() ? bytes[index + 1] : 0;
        const std::uint32_t third = index + 2 < bytes.size() ? bytes[index + 2] : 0;
        const std::uint32_t packed = (first << 16) | (second << 8) | third;
        output.push_back(alphabet[(packed >> 18) & 0x3f]);
        output.push_back(alphabet[(packed >> 12) & 0x3f]);
        output.push_back(index + 1 < bytes.size() ? alphabet[(packed >> 6) & 0x3f] : '=');
        output.push_back(index + 2 < bytes.size() ? alphabet[packed & 0x3f] : '=');
    }
    return output;
}

std::string hexTriple(const std::vector<std::uint8_t>& sps)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(6);
    for (std::size_t index = 1; index <= 3; ++index) {
        result.push_back(digits[sps[index] >> 4]);
        result.push_back(digits[sps[index] & 0x0f]);
    }
    return result;
}

} // namespace

::media::Result<MediaH264SdpCodecDescription>
MediaH264SdpCodecDescriptionFactory::create(const AVCodecParameters& parameters)
{
    if (parameters.codec_type != AVMEDIA_TYPE_VIDEO ||
        parameters.codec_id != AV_CODEC_ID_H264 || !parameters.extradata ||
        parameters.extradata_size <= 0) {
        return ::media::Result<MediaH264SdpCodecDescription>::failure(
            ::media::ErrorInfo::unsupported("final codec parameters are not complete H264"));
    }
    const std::span<const std::uint8_t> bytes(
        parameters.extradata, static_cast<std::size_t>(parameters.extradata_size));
    auto sets = bytes[0] == 1 ? parseAvcc(bytes) : parseAnnexB(bytes);
    if (!sets) {
        return ::media::Result<MediaH264SdpCodecDescription>::failure(sets.error());
    }
    std::string sprop;
    const auto append = [&sprop](const std::vector<std::uint8_t>& nal) {
        if (!sprop.empty()) sprop.push_back(',');
        sprop += base64(nal);
    };
    for (const auto& sps : sets.value().sps) append(sps);
    for (const auto& pps : sets.value().pps) append(pps);
    return ::media::Result<MediaH264SdpCodecDescription>::success(
        MediaH264SdpCodecDescription(
            hexTriple(sets.value().sps.front()), std::move(sprop), 1));
}

} // namespace media::ffmpeg::graph
