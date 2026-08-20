#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedReplayClock.h"
#include "internal/graph/planner/realtime/MediaRtpIngressObservationCollector.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressReceiver.h"
#include "media_transcode/Result.h"

#include <deque>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaPreparedRawRtpDatagram final {
    MediaRtpUdpDatagram datagram;
    std::int64_t observedAtNs;
};

struct MediaPreparedRawRtpIdentity final {
    MediaStreamKind streamKind;
    std::string codecName;
    std::uint8_t payloadType;
    int clockRate;
};

struct MediaPreparedRawRtpInput final {
    MediaRtpUdpTransport transport;
    std::deque<MediaPreparedRawRtpDatagram> datagrams;
    MediaPreparedRawRtpIdentity identity;
    std::optional<MediaDetectedRtpVideoSignaling> videoSignaling;
    std::shared_ptr<MediaRawRtpPreparedReplayClock> replayClock;
    std::shared_ptr<MediaRawRtpPreparedByteBudget> byteBudget;
    std::shared_ptr<MediaRtpIngressObservationCollector> ingressObservation;
    int captureReadTimeoutMs = 0;
};

struct MediaPreparedRawRtpReplayInfo final {
    MediaPreparedRawRtpIdentity identity;
    std::optional<MediaDetectedRtpVideoSignaling> videoSignaling;
    std::size_t rtpDatagrams = 0;
    std::size_t rtcpDatagrams = 0;
    std::int64_t arrivalSpanMilliseconds = 0;
    std::int64_t replayOffsetMilliseconds = 0;
};

class MediaRawRtpPreparedInputBuffer final : public MediaBuffer {
public:
    ~MediaRawRtpPreparedInputBuffer() override;

    static ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>
    create(MediaPreparedRawRtpInput prepared);

    MediaBufferType type() const noexcept override;
    ::media::Status startPreflightCapture();
    ::media::Result<MediaPreparedRawRtpReplayInfo> beginReplay();
    ::media::Result<MediaPreparedRawRtpDatagram> receive(int timeoutMs);
    ::media::Status captureStatus();
    ::media::Result<MediaRtpIngressObservation> ingressObservation();
    ::media::Result<std::size_t> preparedByteCapacity() const;
    ::media::Result<std::size_t> effectiveSocketReceivePayloadBytes() const;
    ::media::Status configureRuntimeIngress(
        const MediaRtpIngressPlan& plan);
    ::media::Status validateRuntimeIngressPlan(
        const MediaRtpIngressPlan& plan) const;
    bool preparedReplayDrained() const noexcept;
    ::media::Result<MediaRtpIngressBatch> receiveRuntimeBatch(
        int timeoutMilliseconds);
    ::media::Status sealPreflight();
    ::media::Status interruptReceive() noexcept;
    ::media::Status stop() noexcept;
    void abort() noexcept;

private:
    explicit MediaRawRtpPreparedInputBuffer(MediaPreparedRawRtpInput prepared);
    void capture(std::stop_token stopToken) noexcept;
    MediaRtpUdpTransport* markStopped() noexcept;

    std::optional<MediaPreparedRawRtpInput> m_prepared;
    std::size_t m_bufferedBytes = 0;
    std::optional<::media::ErrorInfo> m_captureError;
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::jthread m_captureThread;
    std::optional<MediaRawRtpPreparedReplayEpoch> m_replayEpoch;
    std::optional<MediaRtpIngressReceiver> m_runtimeIngress;
    std::optional<MediaRtpIngressPlan> m_runtimeIngressPlan;
    bool m_replayActive = false;
    bool m_preparedQueueConsumed = false;
    bool m_budgetReserved = false;
    bool m_stopped = false;
};

} // namespace media::ffmpeg::graph
