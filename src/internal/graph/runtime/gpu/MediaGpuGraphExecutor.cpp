#include "internal/graph/runtime/gpu/MediaGpuGraphExecutor.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGpuGraphExecutor::prepare(MediaGpuGraphCommandList commands)
{
    if (commands.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGpuGraphExecutor prepare failed: command list is empty"));
    }

    m_commands = std::move(commands);
    m_state = MediaGpuGraphExecutorState::Prepared;
    return ::media::Status::success();
}

::media::Status MediaGpuGraphExecutor::execute()
{
    if (m_state != MediaGpuGraphExecutorState::Prepared &&
        m_state != MediaGpuGraphExecutorState::Completed) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGpuGraphExecutor execute failed: executor is not prepared"));
    }

    m_state = MediaGpuGraphExecutorState::Running;
    m_state = MediaGpuGraphExecutorState::Completed;
    return ::media::Status::success();
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
