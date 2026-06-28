#pragma once

#include "internal/graph/runtime/distributed/MediaGraphDeploymentPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

enum class MediaGraphRemoteExecutorState {
    Idle,
    Deployed,
    Running,
    Stopped,
    Failed
};

class MediaGraphRemoteExecutor final {
public:
    ::media::Status deploy(MediaGraphDeploymentPlan plan);
    ::media::Status start();
    ::media::Status stop();
    void reset();

    MediaGraphRemoteExecutorState state() const noexcept;
    const MediaGraphDeploymentPlan& deploymentPlan() const noexcept;

private:
    MediaGraphDeploymentPlan m_plan;
    MediaGraphRemoteExecutorState m_state = MediaGraphRemoteExecutorState::Idle;
};

} // namespace media::ffmpeg::graph
