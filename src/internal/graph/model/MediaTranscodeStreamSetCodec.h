#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

class MediaTranscodeStreamSetCodec final {
public:
    static ::media::Result<std::string_view> encode(
        MediaTranscodeStreamSet streamSet);
    static ::media::Result<MediaTranscodeStreamSet> decode(
        std::string_view value);
};

} // namespace media::ffmpeg::graph
