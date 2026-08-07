#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace media::ffmpeg::graph {
namespace {

std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return first < last ? std::string(first, last) : std::string{};
}

int base64Value(unsigned char c) noexcept
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace

::media::Result<MediaRtpFmtpParameters> parseRtpFmtp(const std::string& text)
{
    MediaRtpFmtpParameters result;
    std::size_t offset = 0;
    while (offset <= text.size()) {
        const std::size_t separator = text.find(';', offset);
        const std::string token = trim(text.substr(offset, separator == std::string::npos ? std::string::npos : separator - offset));
        if (!token.empty()) {
            const std::size_t equals = token.find('=');
            if (equals == std::string::npos) return ::media::Result<MediaRtpFmtpParameters>::failure(
                ::media::ErrorInfo::invalidArgument("RTP fmtp token requires key=value"));
            const std::string key = lowercaseAscii(
                trim(token.substr(0, equals)));
            const std::string value = trim(token.substr(equals + 1));
            if (key.empty() || value.empty() || result.contains(key)) return ::media::Result<MediaRtpFmtpParameters>::failure(
                ::media::ErrorInfo::invalidArgument("RTP fmtp contains empty or duplicate parameter"));
            result.emplace(key, value);
        }
        if (separator == std::string::npos) break;
        offset = separator + 1;
    }
    return ::media::Result<MediaRtpFmtpParameters>::success(std::move(result));
}

::media::Result<int> requiredRtpFmtpInt(const MediaRtpFmtpParameters& parameters, const std::string& key)
{
    const auto found = parameters.find(lowercaseAscii(key));
    if (found == parameters.end()) return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument("RTP fmtp missing " + key));
    int value = 0;
    const auto parsed = std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != found->second.data() + found->second.size()) return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument("RTP fmtp " + key + " is not an integer"));
    return ::media::Result<int>::success(value);
}

::media::Result<std::vector<uint8_t>> decodeRtpFmtpHex(const std::string& text)
{
    if (text.empty() || (text.size() % 2) != 0) return ::media::Result<std::vector<uint8_t>>::failure(
        ::media::ErrorInfo::invalidArgument("RTP fmtp hex value has invalid length"));
    std::vector<uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        unsigned int value = 0;
        const auto parsed = std::from_chars(text.data() + index, text.data() + index + 2, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + index + 2) return ::media::Result<std::vector<uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument("RTP fmtp hex value is malformed"));
        bytes.push_back(static_cast<uint8_t>(value));
    }
    return ::media::Result<std::vector<uint8_t>>::success(std::move(bytes));
}

::media::Result<std::vector<uint8_t>> decodeRtpFmtpBase64(const std::string& text)
{
    if (text.empty() || (text.size() % 4) != 0) return ::media::Result<std::vector<uint8_t>>::failure(
        ::media::ErrorInfo::invalidArgument("RTP fmtp base64 value has invalid length"));
    std::vector<uint8_t> bytes;
    for (std::size_t offset = 0; offset < text.size(); offset += 4) {
        int values[4]{};
        int padding = 0;
        for (int i = 0; i < 4; ++i) {
            if (text[offset + i] == '=') { values[i] = 0; ++padding; }
            else if ((values[i] = base64Value(static_cast<unsigned char>(text[offset + i]))) < 0 || padding != 0) return ::media::Result<std::vector<uint8_t>>::failure(
                ::media::ErrorInfo::invalidArgument("RTP fmtp base64 value is malformed"));
        }
        const bool noncanonicalPadBits =
            (padding == 2 && (values[1] & 0x0f) != 0) ||
            (padding == 1 && (values[2] & 0x03) != 0);
        if (padding > 2 || (padding != 0 && offset + 4 != text.size()) ||
            noncanonicalPadBits) return ::media::Result<std::vector<uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument("RTP fmtp base64 padding is invalid"));
        const uint32_t value = (static_cast<uint32_t>(values[0]) << 18) | (static_cast<uint32_t>(values[1]) << 12) |
            (static_cast<uint32_t>(values[2]) << 6) | static_cast<uint32_t>(values[3]);
        bytes.push_back(static_cast<uint8_t>(value >> 16));
        if (padding < 2) bytes.push_back(static_cast<uint8_t>(value >> 8));
        if (padding < 1) bytes.push_back(static_cast<uint8_t>(value));
    }
    return ::media::Result<std::vector<uint8_t>>::success(std::move(bytes));
}

::media::Result<std::string> encodeRtpFmtpBase64(
    std::span<const std::uint8_t> bytes)
{
    if (bytes.empty()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP fmtp parameter set cannot be empty"));
    }
    constexpr char Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 3) {
        const std::size_t remaining = bytes.size() - offset;
        const std::uint32_t value =
            (static_cast<std::uint32_t>(bytes[offset]) << 16) |
            (remaining > 1
                 ? static_cast<std::uint32_t>(bytes[offset + 1]) << 8
                 : 0) |
            (remaining > 2 ? static_cast<std::uint32_t>(bytes[offset + 2]) : 0);
        result.push_back(Alphabet[(value >> 18) & 0x3f]);
        result.push_back(Alphabet[(value >> 12) & 0x3f]);
        result.push_back(remaining > 1 ? Alphabet[(value >> 6) & 0x3f] : '=');
        result.push_back(remaining > 2 ? Alphabet[value & 0x3f] : '=');
    }
    return ::media::Result<std::string>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
