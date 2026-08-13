#include "internal/graph/protocol/sdp/MediaSdpBase64Encoder.h"

extern "C" {
#include <libavutil/base64.h>
}

#include <limits>
#include <vector>

namespace media::ffmpeg::graph {

::media::Result<std::string> MediaSdpBase64Encoder::encode(
    std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() ||
        bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument(
                "SDP parameter set must fit the FFmpeg base64 input range"));
    }
    const std::size_t capacity = AV_BASE64_SIZE(bytes.size());
    if (capacity > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument(
                "SDP parameter set base64 output is too large"));
    }
    std::vector<char> output(capacity);
    if (!av_base64_encode(
            output.data(), static_cast<int>(output.size()), bytes.data(),
            static_cast<int>(bytes.size()))) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::internalError(
                "FFmpeg failed to encode an SDP parameter set"));
    }
    return ::media::Result<std::string>::success(
        std::string(output.data()));
}

} // namespace media::ffmpeg::graph
