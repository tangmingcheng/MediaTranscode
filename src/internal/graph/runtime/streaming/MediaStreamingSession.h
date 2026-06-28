#pragma once

#include "internal/graph/preset/MediaPipelinePreset.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/qos/MediaQosController.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaStreamingSessionState {
    Idle,
    Prepared,
    Running,
    Stopped,
    Failed
};

class MediaStreamingSession final {
public:
    ::media::Status prepare(MediaPipelinePresetKind presetKind,
                            const MediaPipelinePresetOptions& options);
    ::media::Status start(bool threaded = false);
    ::media::Status stop();
    void abort() noexcept;

    MediaGraphRuntime& runtime() noexcept;
    const MediaGraphRuntime& runtime() const noexcept;
    MediaStreamingSessionState state() const noexcept;
    MediaGraphRuntimeReport report() const;

private:
    MediaGraphRuntime m_runtime;
    MediaStreamingSessionState m_state = MediaStreamingSessionState::Idle;
};

} // namespace media::ffmpeg::graph
