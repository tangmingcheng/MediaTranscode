#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketTimestampAligner.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

class MediaRtpPacketClockBinderNode final : public FFmpegNodeRuntime {
public:
    explicit MediaRtpPacketClockBinderNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status acceptClock(const MediaBufferRef& buffer);
    ::media::Status checkAcquiringDeadline() const;
    ::media::Status bufferAcquiring(MediaBufferRef buffer);
    ::media::Status bindPacket(MediaGraphExecutionContext& context,
                               MediaBufferRef buffer);
    ::media::Status finishPacketInput(MediaGraphExecutionContext& context,
                                      MediaBufferRef terminal);
    ::media::Result<MediaBufferRef> timedPacket(MediaBufferRef buffer,
                                                std::uint64_t& extendedTimestamp);
    void resetState() noexcept;

    MediaRtpPacketClockProjector m_projector;
    MediaRtpPacketTimestampAligner m_timestampAligner;
    std::optional<MediaRtpClockGroupSnapshot> m_lockedSnapshot;
    std::optional<std::uint64_t> m_lockedGeneration;
    std::deque<MediaBufferRef> m_acquiringPackets;
    std::optional<MediaAvSyncGroupKey> m_syncGroupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::optional<MediaRunningTime> m_acquiringDeadline;
    MediaBufferRef m_videoLookahead;
    std::optional<std::uint64_t> m_videoLookaheadTimestamp;
    std::optional<std::int64_t> m_lastPositiveVideoDelta;
    MediaBufferRef m_pendingTerminal;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    MediaScheduledStream m_scheduledStream = MediaScheduledStream::Video;
    std::size_t m_acquiringCapacity = 0;
    std::int64_t m_acquiringTimeoutNs = 0;
    int m_durationClockRate = 0;
    bool m_configured = false;
};

} // namespace media::ffmpeg::graph
