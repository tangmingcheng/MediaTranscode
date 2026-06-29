#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaChannel;
class MediaGraph;
class MediaGraphExecutionContext;

class MediaGraphRuntimeTrace final {
public:
    static bool enabled(const MediaGraphExecutionContext& context) noexcept;
    static bool enabled(const MediaGraph& graph) noexcept;

    static void compileBegin(const MediaGraph& graph);
    static void compileEnd(const MediaGraphExecutionContext& context);

    static void nodeEnter(const MediaGraphExecutionContext& context,
                          MediaNodeId nodeId,
                          const std::string& runtimeName);
    static void nodeExit(const MediaGraphExecutionContext& context,
                         MediaNodeId nodeId,
                         const std::string& runtimeName,
                         const ::media::Status& status);

    static void edgePush(const MediaGraphExecutionContext& context,
                         const MediaChannel& channel,
                         const MediaBufferRef& buffer,
                         const char* reason = nullptr);
    static void edgePop(const MediaGraphExecutionContext& context,
                        const MediaChannel& channel,
                        const MediaBufferRef& buffer,
                        const char* reason = nullptr);

    static void channelPush(const MediaChannel& channel,
                            const MediaBufferRef& buffer,
                            bool enabled,
                            const ::media::Status& status);
    static void channelTryPush(const MediaChannel& channel,
                               const MediaBufferRef& buffer,
                               bool enabled,
                               bool ok);
    static void channelPop(const MediaChannel& channel,
                           const MediaBufferRef& buffer,
                           bool enabled,
                           const ::media::Status& status);
    static void channelTryPop(const MediaChannel& channel,
                              const MediaBufferRef& buffer,
                              bool enabled,
                              bool ok);
    static void channelState(const MediaChannel& channel,
                             bool enabled,
                             const char* action);

private:
    MediaGraphRuntimeTrace() = default;
};

} // namespace media::ffmpeg::graph
