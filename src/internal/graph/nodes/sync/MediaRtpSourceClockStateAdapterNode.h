#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpSourceClockStateAdapterNode final : public FFmpegNodeRuntime {
public:
    explicit MediaRtpSourceClockStateAdapterNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    struct Projection final {
        MediaSourceClockReadiness readiness;
        std::uint64_t generation;
        bool discontinuity;

        bool operator==(const Projection&) const noexcept = default;
    };

    ::media::Result<MediaNodeProcessResult> emitPendingState(
        MediaGraphExecutionContext& context);
    void resetState() noexcept;

    std::optional<Projection> m_lastEmittedProjection;
    std::optional<Projection> m_pendingProjection;
    MediaBufferRef m_pendingState;
};

} // namespace media::ffmpeg::graph
