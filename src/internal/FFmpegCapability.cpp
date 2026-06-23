#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace media::ffmpeg {
namespace {

    bool hasSuffix(const std::string& value, const char* suffix)
    {
        if (!suffix) {
            return false;
        }

        const std::size_t suffixLen = std::strlen(suffix);
        return value.size() >= suffixLen &&
            value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
    }

} // namespace

    bool isHardwareEncoderName(const char* name)
    {
        if (!name || !*name) {
            return false;
        }

        std::string value(name);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        return hasSuffix(value, "_nvenc") ||
            hasSuffix(value, "_qsv") ||
            hasSuffix(value, "_amf") ||
            hasSuffix(value, "_mf") ||
            hasSuffix(value, "_vaapi") ||
            hasSuffix(value, "_videotoolbox") ||
            hasSuffix(value, "_rkmpp") ||
            hasSuffix(value, "_v4l2m2m") ||
            hasSuffix(value, "_mediacodec") ||
            hasSuffix(value, "_d3d12va");
    }

} // namespace media::ffmpeg
