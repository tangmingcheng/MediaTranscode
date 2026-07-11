#include "internal/graph/runtime/gpu/MediaGpuGraphExecutor.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGpuGraphExecutor::prepare(MediaGpuGraphCommandList)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGpuGraphExecutor is unsupported: generic GPU command execution is not implemented"));
}

::media::Status MediaGpuGraphExecutor::execute()
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGpuGraphExecutor is unsupported: generic GPU command execution is not implemented"));
}

void MediaGpuGraphExecutor::reset()
{
    m_commands.commands.clear();
    m_state = MediaGpuGraphExecutorState::Idle;
}

MediaGpuGraphExecutorState MediaGpuGraphExecutor::state() const noexcept
{
    return m_state;
}

const MediaGpuGraphCommandList& MediaGpuGraphExecutor::commandList() const noexcept
{
    return m_commands;
}

} // namespace media::ffmpeg::graph
