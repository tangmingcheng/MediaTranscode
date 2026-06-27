#pragma once

#include "internal/graph/nodes/MediaNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class FFmpegNodeRuntime : public MediaNodeRuntime {
public:
    FFmpegNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~FFmpegNodeRuntime() override = default;

protected:
    ::media::Result<MediaBufferRef> popInput(MediaGraphExecutionContext& context,
                                             const std::string& portName);
    ::media::Status pushOutput(MediaGraphExecutionContext& context,
                               const std::string& portName,
                               MediaBufferRef buffer);

    ::media::Status forward(MediaGraphExecutionContext& context,
                            const std::string& inputPortName,
                            const std::string& outputPortName);
};

#define MEDIA_FFMPEG_GRAPH_DECLARE_FFMPEG_NODE(ClassName) \
class ClassName final : public FFmpegNodeRuntime { \
public: \
    explicit ClassName(MediaNodeId nodeId); \
    static MediaNodeKind staticKind() noexcept; \
protected: \
    ::media::Status onProcess(MediaGraphExecutionContext& context) override; \
};

#define MEDIA_FFMPEG_GRAPH_DEFINE_FFMPEG_NODE(ClassName, KindValue) \
ClassName::ClassName(MediaNodeId nodeId) \
    : FFmpegNodeRuntime(nodeId, staticKind(), #ClassName) \
{ \
} \
MediaNodeKind ClassName::staticKind() noexcept \
{ \
    return KindValue; \
} \
::media::Status ClassName::onProcess(MediaGraphExecutionContext&) \
{ \
    return ::media::Status::success(); \
}

} // namespace media::ffmpeg::graph
