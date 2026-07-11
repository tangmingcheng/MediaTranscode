#pragma once

#include "internal/graph/model/MediaGraphCapability.h"
#include "internal/graph/runtime/gpu/MediaGpuGraphCommand.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

enum class MediaGpuGraphExecutorState {
    Idle,
    Prepared,
    Running,
    Completed,
    Failed
};

class MediaGpuGraphExecutor final {
public:
    static constexpr MediaCapabilityMaturity capabilityMaturity() noexcept
    {
        return mediaGraphCapabilityMaturity(MediaGraphCapability::GenericGpuExecution);
    }

    ::media::Status prepare(MediaGpuGraphCommandList commands);
    ::media::Status execute();
    void reset();

    MediaGpuGraphExecutorState state() const noexcept;
    const MediaGpuGraphCommandList& commandList() const noexcept;

private:
    MediaGpuGraphCommandList m_commands;
    MediaGpuGraphExecutorState m_state = MediaGpuGraphExecutorState::Idle;
};

} // namespace media::ffmpeg::graph
