#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/MediaGraphPacingClock.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <chrono>

namespace media::ffmpeg::graph {

class MediaVideoOutputSchedulerNode final : public FFmpegNodeRuntime {
public:
    explicit MediaVideoOutputSchedulerNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status validateStartupDeadline() const;
    void resetState() noexcept;

    bool m_configured = false;
    bool m_requireKeyFrame = false;
    bool m_startedMedia = false;
    MediaRunningTime m_maximumStartupWait =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_transportLead =
        MediaRunningTime::fromNanoseconds(0);
    MediaRational m_sourceTimeBase;
    MediaRational m_outputFrameRate;
    std::chrono::steady_clock::time_point m_startedAt{};
    MediaGraphPacingClock m_pacingClock;
};

} // namespace media::ffmpeg::graph
