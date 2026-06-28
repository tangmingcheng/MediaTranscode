#include "internal/graph/runtime/distributed/MediaGraphRemoteExecutor.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphRemoteExecutor::deploy(MediaGraphDeploymentPlan plan)
{
    if (plan.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphRemoteExecutor deploy failed: deployment plan is empty"));
    }

    m_plan = std::move(plan);
    m_state = MediaGraphRemoteExecutorState::Deployed;
    return ::media::Status::success();
}

::media::Status MediaGraphRemoteExecutor::start()
{
    if (m_state != MediaGraphRemoteExecutorState::Deployed &&
        m_state != MediaGraphRemoteExecutorState::Stopped) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRemoteExecutor start failed: executor is not deployed"));
    }

    m_state = MediaGraphRemoteExecutorState::Running;
    return ::media::Status::success();
}

::media::Status MediaGraphRemoteExecutor::stop()
{
    if (m_state == MediaGraphRemoteExecutorState::Idle) {
        return ::media::Status::success();
    }

    m_state = MediaGraphRemoteExecutorState::Stopped;
    return ::media::Status::success();
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
