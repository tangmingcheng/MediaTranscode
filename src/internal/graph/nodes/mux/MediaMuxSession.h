#pragma once

#include "internal/graph/runtime/MediaNodeProcessResult.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaGraphExecutionContext;

struct MediaMuxSessionPollResult final {
    bool progressed;
    std::optional<MediaNodeProcessResult::DeadlineWait> nextWait;
};

class MediaMuxSession {
public:
    virtual ~MediaMuxSession() = default;

    virtual ::media::Status bindResource(MediaGraphExecutionContext& context,
                                         const MediaBufferRef& buffer) = 0;
    virtual ::media::Status bindStreamConfig(MediaGraphExecutionContext& context,
                                             const MediaBufferRef& buffer) = 0;
    virtual ::media::Status write(MediaGraphExecutionContext& context,
                                  const MediaBufferRef& buffer) = 0;
    virtual ::media::Result<MediaMuxSessionPollResult> poll(
        MediaGraphExecutionContext& context) = 0;
    virtual bool hasPendingOutput() const noexcept = 0;
    virtual bool bindingsReady() const noexcept = 0;
    virtual ::media::Status flush(MediaGraphExecutionContext& context) = 0;
    virtual ::media::Status finish(MediaGraphExecutionContext& context) = 0;
    virtual void abort() noexcept = 0;
};

} // namespace media::ffmpeg::graph
