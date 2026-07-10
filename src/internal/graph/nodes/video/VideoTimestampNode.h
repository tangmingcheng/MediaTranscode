#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class VideoTimestampNode final : public FFmpegNodeRuntime {
public:
    explicit VideoTimestampNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindSourceCodecConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status bindTargetCodecConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status normalizeFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    bool m_hasSourceTimeBase = false;
    bool m_hasTargetTimeBase = false;
    bool m_allowSyntheticMissingTimestamps = false;
    int64_t m_lastOutputTimestamp = AV_NOPTS_VALUE;
    AVRational m_sourceTimeBase { 0, 1 };
    AVRational m_targetTimeBase { 0, 1 };
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
};

} // namespace media::ffmpeg::graph
