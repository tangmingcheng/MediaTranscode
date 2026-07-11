#include "internal/graph/runtime/distributed/MediaGraphRemoteExecutor.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRemoteExecutor::deploy(MediaGraphDeploymentPlan)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphRemoteExecutor is unsupported: remote deployment transport is not implemented"));
}

::media::Status MediaGraphRemoteExecutor::start()
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphRemoteExecutor is unsupported: remote execution is not implemented"));
}

::media::Status MediaGraphRemoteExecutor::stop()
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphRemoteExecutor is unsupported: remote execution is not implemented"));
}

void MediaGraphRemoteExecutor::reset()
{
    m_plan.clear();
    m_state = MediaGraphRemoteExecutorState::Idle;
}

MediaGraphRemoteExecutorState MediaGraphRemoteExecutor::state() const noexcept
{
    return m_state;
}

const MediaGraphDeploymentPlan& MediaGraphRemoteExecutor::deploymentPlan() const noexcept
{
    return m_plan;
}

} // namespace media::ffmpeg::graph
