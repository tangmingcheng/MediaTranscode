#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaSocketRuntime final {
public:
    ~MediaSocketRuntime();

    MediaSocketRuntime(const MediaSocketRuntime&) = delete;
    MediaSocketRuntime& operator=(const MediaSocketRuntime&) = delete;

    static ::media::Result<std::shared_ptr<MediaSocketRuntime>> create();

private:
    explicit MediaSocketRuntime(bool initialized) noexcept;

    bool m_initialized;
};

} // namespace media::ffmpeg::graph
