#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/MediaNodeProcessResult.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphExecutionContext;

class MediaRuntimeNode {
public:
    virtual ~MediaRuntimeNode() = default;

    MediaRuntimeNode(const MediaRuntimeNode&) = delete;
    MediaRuntimeNode& operator=(const MediaRuntimeNode&) = delete;

    virtual MediaNodeId nodeId() const noexcept = 0;

    virtual ::media::Status configure(MediaGraphExecutionContext& context);
    virtual ::media::Status start(MediaGraphExecutionContext& context);
    virtual ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) = 0;
    virtual ::media::Status flush(MediaGraphExecutionContext& context);
    virtual ::media::Status stop(MediaGraphExecutionContext& context);
    virtual void interrupt(MediaGraphExecutionContext& context) noexcept;
    virtual void abort(MediaGraphExecutionContext& context) noexcept;

protected:
    MediaRuntimeNode() = default;
};

} // namespace media::ffmpeg::graph
