#pragma once

#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"

namespace media::ffmpeg::graph {

class MediaPosixAtomicFileReplacePort final : public MediaAtomicFileReplacePort {
public:
    ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>> begin(
        std::string_view targetPathUtf8) override;
};

} // namespace media::ffmpeg::graph
