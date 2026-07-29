#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include <atomic>

namespace media::ffmpeg::graph {

class MediaScheduledOutputRouterNode final : public FFmpegNodeRuntime {
public:
    explicit MediaScheduledOutputRouterNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    enum class Mode : std::uint8_t { SplitAv, SerializedAv };
    ::media::Status configureTopology(
        MediaGraphExecutionContext& context) const;
    ::media::Result<MediaNodeProcessResult> routeScheduledUnit(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> routeControl(
        MediaGraphExecutionContext& context,
        MediaControlBufferKind kind);
    void resetState() noexcept;

    MediaBufferRef m_pending;
    Mode m_mode = Mode::SplitAv;
    std::atomic_bool m_interrupted{false};
};

} // namespace media::ffmpeg::graph
