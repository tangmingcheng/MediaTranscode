#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"
#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"
#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"

#include <atomic>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MpegTsDemuxNode final : public FFmpegNodeRuntime {
public:
    explicit MpegTsDemuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;

private:
    struct StreamClock {
        std::optional<MediaTsSourceClockMapper> mapper;
        std::optional<std::uint64_t> generation;
        MediaSourceClockReadiness readiness = MediaSourceClockReadiness::Acquiring;
    };

    ::media::Status bind(MediaGraphExecutionContext& context);
    ::media::Result<MediaPacketSourceTiming> timingFor(
        const AVPacket& packet, const MediaTsClockProjectionCheckpoint& checkpoint,
        StreamClock& clock);
    ::media::Status emitEof(MediaGraphExecutionContext& context);
    void reset() noexcept;

    std::unique_ptr<MediaTsInputSession> m_session;
    std::optional<MediaTsClockProjection> m_projection;
    std::optional<MediaTsProgramClockPolicy> m_policy;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    StreamClock m_videoClock;
    StreamClock m_audioClock;
    bool m_eofSent = false;
    std::atomic_bool m_aborted{false};
};

} // namespace media::ffmpeg::graph
