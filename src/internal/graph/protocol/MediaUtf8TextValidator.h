#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaUtf8ControlPolicy {
    RejectControls,
    AllowCrLf
};

class MediaUtf8TextValidator final {
public:
    static ::media::Status validateWellFormed(
        std::string_view text,
        MediaUtf8ControlPolicy controlPolicy);

    static ::media::Status validateNonControlText(
        std::string_view text,
        std::size_t maximumBytes,
        std::string_view fieldName);

private:
    MediaUtf8TextValidator() = delete;
};

} // namespace media::ffmpeg::graph
