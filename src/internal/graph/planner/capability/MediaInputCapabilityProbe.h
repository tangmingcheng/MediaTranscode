#pragma once
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include <media_transcode/Result.h>
#include <string>
struct AVDictionary;
namespace media::ffmpeg::graph {
class MediaInputCapabilityProbe final {
public:
    static ::media::Result<::media::ffmpeg::InputFormatContextPtr> open(const std::string& inputUrl, AVDictionary** inputOptions);
private:
    MediaInputCapabilityProbe() = delete;
};
}
