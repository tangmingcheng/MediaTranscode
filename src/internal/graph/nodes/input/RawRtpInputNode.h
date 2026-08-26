#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadReservation.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressBatch.h"

#include <deque>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class RawRtpInputNode final : public FFmpegNodeRuntime {
public:
    explicit RawRtpInputNode(MediaNodeId nodeId);
    RawRtpInputNode(MediaNodeId nodeId,
                    MediaPreparedRealtimeInput prepared);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status prepareReceiver(MediaGraphExecutionContext& context);
    ::media::Status processRtp(MediaGraphExecutionContext& context,
                               std::span<const std::uint8_t> datagram,
                               std::int64_t observedAtNs);
    ::media::Status processRtcp(MediaGraphExecutionContext& context,
                                std::span<const std::uint8_t> datagram,
                                std::int64_t observedAtNs);
    ::media::Status processReordered(MediaGraphExecutionContext& context,
                                     MediaRtpReorderResult reordered,
                                     std::uint64_t generationBeforeObservation);
    ::media::Status drainPendingRtpPackets(
        MediaGraphExecutionContext& context);
    ::media::Status processPendingRtpPacket(
        MediaGraphExecutionContext& context,
        const MediaRtpPacket& packet);
    ::media::Status queueClockEvidence(MediaGraphExecutionContext& context,
                                       std::int64_t observedAtNs);
    ::media::Status queueClockTransition(MediaGraphExecutionContext& context,
                                         std::int64_t observedAtNs);
    void resetState() noexcept;
    void logIngressBatchTelemetry() const;
    std::uint64_t nextIngressSequence() noexcept;

    MediaRtpUdpTransport m_transport;
    MediaPreparedRealtimeInput m_prepared;
    std::shared_ptr<MediaRawRtpPreparedInputBuffer> m_preparedReceiver;
    std::optional<MediaRtpIngressBatch> m_runtimeIngressBatch;
    std::size_t m_runtimeIngressBatchIndex = 0;
    std::uint64_t m_runtimeIngressBatches = 0;
    std::uint64_t m_runtimeIngressDatagrams = 0;
    std::uint64_t m_runtimeIngressBytes = 0;
    std::size_t m_runtimeIngressMaximumBatchDatagrams = 0;
    std::size_t m_runtimeIngressMaximumBatchBytes = 0;
    std::unique_ptr<MediaRtpReorderBuffer> m_reorder;
    std::unique_ptr<MediaRtpDepacketizer> m_depacketizer;
    std::unique_ptr<MediaRtcpSenderReportTracker> m_clockTracker;
    std::unique_ptr<MediaRtpClockObservationSchedule> m_clockSchedule;
    MediaRtpDepacketizerConfig m_config;
    MediaPreparedRtpAccessUnitEnvelope m_accessUnitEnvelope;
    std::deque<MediaRtpPacket> m_pendingRtpPackets;
    std::vector<MediaGraphPayloadReservation> m_pendingPayloadReservations;
    std::optional<std::uint32_t> m_reservedAccessUnitTimestamp;
    MediaBufferRef m_streamSnapshot;
    std::deque<MediaBufferRef> m_packets;
    std::deque<std::pair<std::string, MediaBufferRef>> m_events;
    bool m_initialized = false;
    bool m_requiresPreparedInput = false;
    bool m_formatEmitted = false;
    bool m_keyTraceEmitted = false;
    bool m_requireCname = false;
    std::optional<MediaRtcpCompositionMode> m_rtcpCompositionMode;
    int m_cancellableReadTimeoutMs = 0;
    std::uint64_t m_nextIngressSequence = 1;
};

} // namespace media::ffmpeg::graph
