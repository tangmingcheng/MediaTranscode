#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWith(const std::string& value, const char* prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool endsWith(const std::string& value, const char* suffix)
{
    const std::string suffixText(suffix);
    return value.size() >= suffixText.size() &&
           value.compare(value.size() - suffixText.size(), suffixText.size(), suffixText) == 0;
}

std::string stripUrlQueryAndFragment(std::string value)
{
    const std::size_t query = value.find_first_of("?#");
    if (query != std::string::npos) {
        value.resize(query);
    }
    return value;
}

::media::Result<uint16_t> parsePortText(const std::string& text)
{
    if (text.empty()) {
        return ::media::Result<uint16_t>::failure(
            ::media::ErrorInfo::invalidArgument("RTP/UDP URL requires explicit port"));
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed == 0 || parsed > std::numeric_limits<uint16_t>::max()) {
        return ::media::Result<uint16_t>::failure(
            ::media::ErrorInfo::invalidArgument("RTP/UDP URL port must be in range 1..65535"));
    }
    return ::media::Result<uint16_t>::success(static_cast<uint16_t>(parsed));
}

} // namespace

std::string redactUrlUserInfo(const std::string& url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string::npos) {
        return url;
    }

    const std::size_t authorityBegin = scheme + 3;
    const std::size_t authorityEnd = url.find_first_of("/?#", authorityBegin);
    const std::size_t userInfoEnd = url.find('@', authorityBegin);
    if (userInfoEnd == std::string::npos ||
        (authorityEnd != std::string::npos && userInfoEnd > authorityEnd)) {
        return url;
    }

    return url.substr(0, authorityBegin) + "<redacted>@" + url.substr(userInfoEnd + 1);
}

bool isUnsupportedRealtimeInputUrl(const std::string& url)
{
    const std::string normalized = stripUrlQueryAndFragment(lowerAscii(url));
    return startsWith(normalized, "rtp://") ||
           startsWith(normalized, "udp://") ||
           startsWith(normalized, "sdp://") ||
           endsWith(normalized, ".sdp");
}

bool isUdpUrl(const std::string& url)
{
    return startsWith(lowerAscii(url), "udp://");
}

::media::Result<MediaRtpUrlEndpoint> parseRtpUdpUrlEndpoint(const std::string& url)
{
    const std::string normalized = url;
    const std::string lower = lowerAscii(normalized);
    const std::size_t schemeEnd = lower.find("://");
    if (schemeEnd == std::string::npos) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("RTP/UDP URL requires scheme"));
    }

    MediaRtpUrlEndpoint endpoint;
    endpoint.scheme = lower.substr(0, schemeEnd);
    if (endpoint.scheme != "rtp" && endpoint.scheme != "udp") {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input requires rtp:// or udp:// URL"));
    }

    const std::size_t authorityBegin = schemeEnd + 3;
    if (normalized.find_first_of("/?#", authorityBegin) != std::string::npos) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input URL must be exactly rtp://host:port or udp://host:port"));
    }

    const std::string authority = normalized.substr(authorityBegin);
    if (authority.find('@') != std::string::npos) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input URL must be exactly rtp://host:port or udp://host:port"));
    }

    const std::size_t colon = authority.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= authority.size()) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input URL requires host and port"));
    }

    endpoint.host = authority.substr(0, colon);
    auto port = parsePortText(authority.substr(colon + 1));
    if (!port) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(port.error());
    }
    endpoint.port = port.value();
    return ::media::Result<MediaRtpUrlEndpoint>::success(std::move(endpoint));
}

} // namespace media::ffmpeg::graph
