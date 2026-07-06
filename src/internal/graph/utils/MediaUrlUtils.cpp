#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <cctype>

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

} // namespace media::ffmpeg::graph
