#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/nodes/mux/MediaMuxSession.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaMuxSessionFactory {
public:
    virtual ~MediaMuxSessionFactory() = default;

    virtual ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions& options) const = 0;
};

class ExplicitMediaMuxSessionFactory final : public MediaMuxSessionFactory {
public:
    ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions& options) const override;
};

} // namespace media::ffmpeg::graph
