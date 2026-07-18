#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

class MediaProjectMpegTsPlanSourceNode final : public FFmpegNodeRuntime {
public:
    MediaProjectMpegTsPlanSourceNode(MediaNodeId nodeId,
                                     MediaAvSyncGroupKey group,
                                     MediaTsMuxPlan plan);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    void resetState() noexcept;

    MediaAvSyncGroupKey m_group;
    MediaTsMuxPlan m_plan;
    MediaBufferRef m_pendingPlan;
    bool m_published = false;
};

} // namespace media::ffmpeg::graph
