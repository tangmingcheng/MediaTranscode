#pragma once

#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "media_transcode/Result.h"

#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvSyncSourceClockModeNodeOptionCodec final {
public:
    static ::media::Result<std::string> encode(
        MediaAvSyncSourceClockMode mode);
    static ::media::Result<MediaAvSyncSourceClockMode> decode(
        std::string_view text);

private:
    MediaAvSyncSourceClockModeNodeOptionCodec() = delete;
};

} // namespace media::ffmpeg::graph
