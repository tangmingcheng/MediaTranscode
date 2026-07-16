#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaSourceClockStateBuffer;

class MediaAvStartupClockNode final : public FFmpegNodeRuntime {
public:
    explicit MediaAvStartupClockNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status observe(const MediaSourceClockStateBuffer& state);
    void resetState() noexcept;

    std::optional<MediaAvSyncGroupKey> m_groupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::optional<MediaRunningTime> m_interval;
    std::optional<MediaRunningTime> m_nextTick;
    std::optional<std::uint64_t> m_generation;
};

} // namespace media::ffmpeg::graph
