#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaScheduledTsAccessUnitAdapterNode final : public FFmpegNodeRuntime {
public:
    MediaScheduledTsAccessUnitAdapterNode(MediaNodeId nodeId,
                                          MediaAvSyncGroupKey group);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    void resetState() noexcept;

    MediaAvSyncGroupKey m_group;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaRunningTime> m_transportLead;
};

} // namespace media::ffmpeg::graph
