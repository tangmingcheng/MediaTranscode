#pragma once

#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaNodeRuntime : public MediaRuntimeNode {
public:
    MediaNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~MediaNodeRuntime() override = default;

    MediaNodeId nodeId() const noexcept override;
    MediaNodeKind kind() const noexcept;
    const std::string& name() const noexcept;

    ::media::Status process(MediaGraphExecutionContext& context) override;

protected:
    virtual ::media::Status onProcess(MediaGraphExecutionContext& context);

private:
    MediaNodeId m_nodeId;
    MediaNodeKind m_kind = MediaNodeKind::Unknown;
    std::string m_name;
};

#define MEDIA_FFMPEG_GRAPH_DECLARE_NODE(ClassName) \
class ClassName final : public MediaNodeRuntime { \
public: \
    explicit ClassName(MediaNodeId nodeId); \
    static MediaNodeKind staticKind() noexcept; \
protected: \
    ::media::Status onProcess(MediaGraphExecutionContext& context) override; \
};

#define MEDIA_FFMPEG_GRAPH_DEFINE_NODE(ClassName, KindValue) \
ClassName::ClassName(MediaNodeId nodeId) \
    : MediaNodeRuntime(nodeId, staticKind(), #ClassName) \
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
