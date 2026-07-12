#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <deque>
#include <memory>

namespace media::ffmpeg::graph {

class RawRtpInputNode final : public FFmpegNodeRuntime {
public:
    explicit RawRtpInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status initialize(MediaGraphExecutionContext& context);
    ::media::Status processRtp(MediaGraphExecutionContext& context, MediaRtpUdpDatagram datagram);
    ::media::Status processRtcp(MediaGraphExecutionContext& context, MediaRtpUdpDatagram datagram);
    ::media::Status processReordered(MediaGraphExecutionContext& context,
                                     MediaRtpReorderResult reordered,
                                     std::uint64_t generationBeforeObservation);
    ::media::Status queueClockTransition(MediaGraphExecutionContext& context,
                                         std::int64_t observedAtNs);
    void resetState() noexcept;

    MediaRtpUdpTransport m_transport;
    std::unique_ptr<MediaRtpReorderBuffer> m_reorder;
    std::unique_ptr<MediaRtpDepacketizer> m_depacketizer;
    std::unique_ptr<MediaRtcpSenderReportTracker> m_clockTracker;
    std::unique_ptr<MediaRtpClockObservationSchedule> m_clockSchedule;
    MediaRtpDepacketizerConfig m_config;
    MediaBufferRef m_streamSnapshot;
    std::deque<MediaBufferRef> m_packets;
    std::deque<std::pair<std::string, MediaBufferRef>> m_events;
    bool m_initialized = false;
    bool m_formatEmitted = false;
    bool m_requireCname = false;
    int m_cancellableReadTimeoutMs = 0;
};

} // namespace media::ffmpeg::graph
