#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <exception>

namespace media::ffmpeg::graph {

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
    auto text = requiredNodeOption(options, nodeName, key);
    if (!text) {
        return ::media::Result<int>::failure(text.error());
    }
    try {
        const int parsed = std::stoi(text.value());
        if (parsed > 0) {
            return ::media::Result<int>::success(parsed);
        }
    } catch (const std::exception&) {
    }
    return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(nodeName) + " requires positive integer option: " + key));
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

} // namespace media::ffmpeg::graph
