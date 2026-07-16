#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

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
    ::media::Status bufferAcquiring(MediaBufferRef buffer);
    ::media::Status bindPacket(MediaGraphExecutionContext& context,
                               MediaBufferRef buffer);
    ::media::Status finishPacketInput(MediaGraphExecutionContext& context,
                                      MediaBufferRef terminal);
    ::media::Result<MediaBufferRef> timedPacket(MediaBufferRef buffer,
                                                std::uint64_t& extendedTimestamp);
    void resetState() noexcept;

    MediaRtpPacketClockProjector m_projector;
    std::optional<MediaRtpClockGroupSnapshot> m_lockedSnapshot;
    std::optional<std::uint64_t> m_lockedGeneration;
    std::deque<MediaBufferRef> m_acquiringPackets;
    std::optional<std::chrono::steady_clock::time_point> m_acquiringStarted;
    MediaBufferRef m_videoLookahead;
    std::optional<std::uint64_t> m_videoLookaheadTimestamp;
    std::optional<std::int64_t> m_lastPositiveVideoDelta;
    MediaBufferRef m_pendingTerminal;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    MediaScheduledStream m_scheduledStream = MediaScheduledStream::Video;
    std::size_t m_acquiringCapacity = 0;
    std::chrono::nanoseconds m_acquiringTimeout{0};
    int m_durationClockRate = 0;
    bool m_configured = false;
};

} // namespace media::ffmpeg::graph
