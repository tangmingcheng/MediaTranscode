#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/nodes/MediaNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class RouteMatchPolicy {
    RequireMatch,
    AllowDrop
};

class FFmpegNodeRuntime : public MediaNodeRuntime {
public:
    FFmpegNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~FFmpegNodeRuntime() override = default;

protected:
    struct PoppedChannelBuffer {
        MediaChannel* channel = nullptr;
        MediaBufferRef buffer;
    };
    const MediaNodeOptions* nodeOptions(MediaGraphExecutionContext& context) const noexcept;
    std::string nodeOption(MediaGraphExecutionContext& context,
                           const std::string& key,
                           std::string missingValue = {}) const;

    ::media::Result<MediaBufferRef> popInput(MediaGraphExecutionContext& context,
                                             const std::string& portName);
    ::media::Result<MediaBufferRef> tryPopFirstInput(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<MediaBufferRef>> tryPopFirstInputOptional(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<PoppedChannelBuffer>> tryPopFirstInputWithChannelOptional(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<MediaBufferRef>> tryPopInputOptional(MediaGraphExecutionContext& context,
                                                                        const std::string& portName);

    ::media::Status emitOutput(MediaGraphExecutionContext& context,
                               const std::string& portName,
                               const MediaBufferRef& buffer);
    ::media::Status pushToAllOutputs(MediaGraphExecutionContext& context,
                                      const MediaBufferRef& buffer);
    ::media::Status broadcastControlToAllOutputs(MediaGraphExecutionContext& context,
                                                  const MediaBufferRef& buffer);
    ::media::Status pushToMatchingOutputs(MediaGraphExecutionContext& context,
                                           const MediaBufferRef& buffer,
                                           MediaStreamKind streamKind,
                                           int streamIndex = invalidMediaStreamIndex,
                                           RouteMatchPolicy policy = RouteMatchPolicy::RequireMatch);

    std::vector<MediaChannel*> outputChannels(MediaGraphExecutionContext& context);
};

#define MEDIA_FFMPEG_GRAPH_DECLARE_FFMPEG_NODE(ClassName) \
class ClassName final : public FFmpegNodeRuntime { \
public: \
    explicit ClassName(MediaNodeId nodeId); \
    static MediaNodeKind staticKind() noexcept; \
protected: \
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override; \
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
::media::Result<MediaNodeProcessResult> ClassName::onProcess(MediaGraphExecutionContext&) \
{ \
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished()); \
}

} // namespace media::ffmpeg::graph
