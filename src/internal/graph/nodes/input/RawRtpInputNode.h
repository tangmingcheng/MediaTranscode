#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaRtpIngressFrontiers.h"
#include "internal/graph/protocol/rtp/MediaRtpInputProtocolSession.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
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
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaPreparedRtpSourceSession;

class RawRtpInputNode final : public FFmpegNodeRuntime {
public:
    RawRtpInputNode(MediaNodeId nodeId,
                    std::optional<MediaRtpIngressBinding> ingressBinding);
    RawRtpInputNode(MediaNodeId nodeId,
                    MediaPreparedRealtimeInput prepared,
                    std::optional<MediaRtpIngressBinding> ingressBinding);
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;
    std::string_view generationPurgeIdentity() const noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    ::media::Status bindPreparedSession(std::shared_ptr<MediaPreparedRtpSourceSession> source);
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
    ::media::Status drainPendingRtpItems(
        MediaGraphExecutionContext& context);
    ::media::Status processPendingRtpPacket(
        MediaGraphExecutionContext& context,
        const MediaRtpPacket& packet);
    ::media::Status publishClockObservation(MediaGraphExecutionContext& context,
        MediaRtpPreparedClockObservation prepared, std::int64_t evaluationAtNs, const MediaRtpDiscontinuity* discontinuity);
    ::media::Status queueClockEvidence(MediaGraphExecutionContext& context,
                                       std::int64_t observedAtNs);
    ::media::Status queueClockTransition(MediaGraphExecutionContext& context,
                                         std::int64_t observedAtNs);
    void discardUnassembledPackets() noexcept;
    void resetState() noexcept;
    void logIngressBatchTelemetry() const;
    std::uint64_t nextIngressSequence() noexcept;

    const std::optional<MediaRtpIngressBinding> m_ingressBinding;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_ownerPurge;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    MediaRtpUdpTransport m_transport;
    MediaPreparedRealtimeInput m_prepared;
    std::shared_ptr<MediaRawRtpPreparedInputBuffer> m_preparedReceiver;
    std::shared_ptr<MediaPreparedRtpSourceSession> m_preparedSource;
    MediaBufferRef m_preparedPendingPacket;
    std::optional<MediaRtpIngressBatch> m_runtimeIngressBatch;
    std::size_t m_runtimeIngressBatchIndex = 0;
    bool m_runtimeBatchMediaRetired = false;
    std::uint64_t m_runtimeIngressBatches = 0;
    std::uint64_t m_runtimeIngressDatagrams = 0;
    std::uint64_t m_runtimeIngressBytes = 0;
    std::size_t m_runtimeIngressMaximumBatchDatagrams = 0;
    std::size_t m_runtimeIngressMaximumBatchBytes = 0;
    std::unique_ptr<MediaRtpInputProtocolSession> m_protocol;
    MediaRtpDepacketizerConfig m_config;
    MediaPreparedRtpAccessUnitEnvelope m_accessUnitEnvelope;
    std::vector<MediaGraphPayloadReservation> m_pendingPayloadReservations;
    MediaBufferRef m_streamSnapshot;
    std::deque<MediaBufferRef> m_packets;
    std::deque<std::pair<std::string, MediaBufferRef>> m_events;
    bool m_initialized = false;
    bool m_requiresPreparedInput = false;
    bool m_formatEmitted = false;
    bool m_keyTraceEmitted = false;
    int m_cancellableReadTimeoutMs = 0;
    std::uint64_t m_nextIngressSequence = 1;
};

} // namespace media::ffmpeg::graph
