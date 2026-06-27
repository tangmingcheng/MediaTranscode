#include "internal/graph/nodes/MediaNodeRuntime.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaNodeRuntime::MediaNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : m_nodeId(nodeId)
    , m_kind(kind)
    , m_name(std::move(name))
{
}

MediaNodeId MediaNodeRuntime::nodeId() const noexcept
{
    return m_nodeId;
}

MediaNodeKind MediaNodeRuntime::kind() const noexcept
{
    return m_kind;
}

const std::string& MediaNodeRuntime::name() const noexcept
{
    return m_name;
}

::media::Status MediaNodeRuntime::process(MediaGraphExecutionContext& context)
{
    return onProcess(context);
}

::media::Status MediaNodeRuntime::onProcess(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
