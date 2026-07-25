#include "internal/graph/planner/capability/MediaInputCapabilityProbe.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}
namespace media::ffmpeg::graph {
::media::Result<::media::ffmpeg::InputFormatContextPtr> MediaInputCapabilityProbe::open(const std::string& inputUrl, AVDictionary** inputOptions)
{
    AVFormatContext* raw = nullptr;
    const int result = avformat_open_input(&raw, inputUrl.c_str(), nullptr, inputOptions);
    if (result < 0) {
        char text[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(result, text, sizeof(text));
        return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_open_input: " + std::string(text), result));
    }
    return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::success(::media::ffmpeg::InputFormatContextPtr(raw));
}
}
