#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <algorithm>
#include <cctype>
#include <exception>

namespace media::ffmpeg::graph {
namespace {

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

::media::Result<int> requiredIntNodeOption(const MediaNodeOptions* options,
                                           const char* nodeName,
                                           const char* key)
{
    auto text = requiredNodeOption(options, nodeName, key);
    if (!text) {
        return ::media::Result<int>::failure(text.error());
    }
    try {
        std::size_t parsedLength = 0;
        const int parsed = std::stoi(text.value(), &parsedLength, 10);
        if (parsedLength == text.value().size()) {
            return ::media::Result<int>::success(parsed);
        }
    } catch (const std::exception&) {
    }
    return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires integer option: " + key));
}

} // namespace

::media::Result<std::string> requiredNodeOption(const MediaNodeOptions* options,
                                                const char* nodeName,
                                                const char* key)
{
    if (!options || !options->has(key)) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires node option: " + key));
    }
    const std::string value = options->value(key);
    if (value.empty()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires non-empty node option: " + key));
    }
    return ::media::Result<std::string>::success(value);
}

::media::Result<int> requiredPositiveIntNodeOption(const MediaNodeOptions* options,
                                                   const char* nodeName,
                                                   const char* key)
{
    auto parsed = requiredIntNodeOption(options, nodeName, key);
    if (!parsed) {
        return parsed;
    }
    if (parsed.value() > 0) {
        return parsed;
    }
    return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires positive integer option: " + key));
}

::media::Result<int> requiredNonNegativeIntNodeOption(const MediaNodeOptions* options,
                                                      const char* nodeName,
                                                      const char* key)
{
    auto parsed = requiredIntNodeOption(options, nodeName, key);
    if (!parsed) {
        return parsed;
    }
    if (parsed.value() >= 0) {
        return parsed;
    }
    return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires non-negative integer option: " + key));
}

::media::Result<bool> requiredBoolNodeOption(const MediaNodeOptions* options,
                                             const char* nodeName,
                                             const char* key)
{
    auto text = requiredNodeOption(options, nodeName, key);
    if (!text) {
        return ::media::Result<bool>::failure(text.error());
    }
    const std::string& value = text.value();
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return ::media::Result<bool>::success(true);
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return ::media::Result<bool>::success(false);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires boolean option: " + key));
}

::media::Result<MediaStreamKind> requiredStreamKindNodeOption(const MediaNodeOptions* options,
                                                              const char* nodeName,
                                                              const char* key)
{
    auto text = requiredNodeOption(options, nodeName, key);
    if (!text) {
        return ::media::Result<MediaStreamKind>::failure(text.error());
    }
    const std::string value = lowerCopy(text.value());
    if (value == "video") {
        return ::media::Result<MediaStreamKind>::success(MediaStreamKind::Video);
    }
    if (value == "audio") {
        return ::media::Result<MediaStreamKind>::success(MediaStreamKind::Audio);
    }
    return ::media::Result<MediaStreamKind>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " supports " + key + " values: video, audio"));
}

} // namespace media::ffmpeg::graph
