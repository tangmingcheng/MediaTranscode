#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h"
#include "internal/graph/runtime/buffer/MediaRawRtpProbeLease.h"
#include "internal/graph/runtime/buffer/MediaPreparedRtpDatagramStorage.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedReplayClock.h"
#include "internal/graph/planner/realtime/MediaRtpIngressObservationCollector.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressSnapshot.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressReceiver.h"
#include "media_transcode/Result.h"

#include <deque>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaPreparedRawRtpReplayDatagram final {
    MediaPreparedRtpDatagramOwner storage;
    std::int64_t observedAtNs;
};

struct MediaPreparedRawRtpIdentity final {
    MediaStreamKind streamKind;
    std::string codecName;
    std::uint8_t payloadType;
    int clockRate;
};

struct MediaPreparedRawRtpInput final {
    // The resource owner outlives even empty containers and transport scratch.
    std::shared_ptr<MediaRawRtpPreparedByteBudget> byteBudget;
    MediaRtpUdpTransport transport;
    MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner> datagrams;
    MediaPreparedRawRtpIdentity identity;
    std::optional<MediaDetectedRtpVideoSignaling> videoSignaling;
    std::shared_ptr<MediaRawRtpPreparedReplayClock> replayClock;
    std::shared_ptr<MediaRtpIngressObservationCollector> ingressObservation;
    int captureReadTimeoutMs = 0;
    std::uint64_t ingressFrontier = 0;
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
    ::media::Result<MediaRawRtpProbeLease> acquireProbeLease(
        const std::shared_ptr<const MediaRawRtpPreparedInputBuffer>& owner) const;
    ::media::Status waitForProbeData(std::uint64_t previousIngressFrontier,
        std::chrono::steady_clock::time_point deadline,
        std::stop_token stopToken);
    ::media::Result<MediaPreparedRawRtpReplayInfo> beginReplay();
    ::media::Status beginContinuousRuntime(
        const std::shared_ptr<class MediaPreparedRtpSourceSession>& source);
    ::media::Result<MediaPreparedRawRtpReplayDatagram> receive(int timeoutMs);
    ::media::Status captureStatus();
    ::media::Result<MediaRtpIngressObservation> ingressObservation();
    ::media::Result<MediaPreparedRtpIngressSnapshot> ingressSnapshot(
        const std::shared_ptr<const MediaRawRtpPreparedInputBuffer>& owner,
        const std::optional<MediaRtpIngressSnapshotWait>& wait);
    ::media::Result<std::size_t> preparedByteCapacity() const;
    ::media::Result<MediaPreparedRtpStorageReservation> preparationStorage() const;
    ::media::Result<std::uint64_t> sealedReplayAccessUnitBound() const;
    ::media::Result<std::size_t> maximumDatagramBytes() const;
    ::media::Status configureRuntimeIngress(
        const MediaRtpIngressPlan& plan);
    ::media::Status validateRuntimeIngressPlan(
        const MediaRtpIngressPlan& plan) const;
    bool preparedReplayDrained() const noexcept;
    ::media::Status discardReplayMedia();
    ::media::Result<MediaRtpIngressBatch> receiveRuntimeBatch(
        int timeoutMilliseconds);
    ::media::Status finishPreflightCapture();
    ::media::Status interruptReceive() noexcept;
    ::media::Status stop() noexcept;
    void abort() noexcept;

private:
    friend class MediaRtpSourcePreflight;
    struct CaptureTermination final {
        enum class Reason { CapacityExhausted };
        Reason reason;
        std::size_t wireCapacityBytes;
        std::size_t observedWireBytes;
        std::size_t maximumDatagramBytes;
    };
    ::media::Status continuousPreparationStatusLocked(
        const std::shared_ptr<class MediaPreparedRtpSourceSession>& expected) const;
    ::media::Result<std::uint64_t> accessUnitBoundLocked() const;
    explicit MediaRawRtpPreparedInputBuffer(MediaPreparedRawRtpInput prepared);
    void capture(std::stop_token stopToken) noexcept;
    ::media::Status captureContinuousBatch(std::stop_token stopToken);
    MediaRtpUdpTransport* markStopped() noexcept;

    std::optional<MediaPreparedRawRtpInput> m_prepared;
    std::size_t m_bufferedBytes = 0;
    std::optional<::media::ErrorInfo> m_captureError;
    std::optional<CaptureTermination> m_captureTermination;
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::jthread m_captureThread;
    std::optional<MediaRawRtpPreparedReplayEpoch> m_replayEpoch;
    std::optional<MediaRtpIngressReceiver> m_runtimeIngress;
    std::optional<MediaRtpIngressPlan> m_runtimeIngressPlan;
    std::shared_ptr<class MediaPreparedRtpSourceSession> m_continuousSource;
    std::optional<MediaRtpIngressPlan> m_continuousIngressPlan;
    bool m_replayActive = false;
    bool m_preparedQueueConsumed = false;
    bool m_stopped = false;
    bool m_captureStarted = false;
    bool m_captureFinished = false;
};

} // namespace media::ffmpeg::graph
