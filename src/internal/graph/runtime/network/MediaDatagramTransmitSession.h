#pragma once

#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDatagramTransmitEndpointBinding final {
    std::uint64_t endpointId;
    MediaUdpDatagramEndpoint localEndpoint;
};

struct MediaDatagramTransmitExecutionPlan final {
    MediaDatagramTransmitExecutionMode mode;
    std::string authority;
    std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule;
    std::optional<std::uint64_t> kernelSocketPacingRateBytesPerSecond;
};

struct MediaDatagramTransmitJobEntry final {
    std::span<const std::uint8_t> bytes;
    std::uint64_t evidenceId = 0;
    MediaRunningTime enqueueNotAfter = MediaRunningTime::fromNanoseconds(0);
    std::optional<std::uint64_t> kernelTransmitTimeNanoseconds;
};

class MediaDatagramTransmitSession final {
public:
    // The creating thread is the sole owner through close and destruction.
    // Public operations are non-concurrent and may not migrate threads.
    ~MediaDatagramTransmitSession() noexcept;

    MediaDatagramTransmitSession(const MediaDatagramTransmitSession&) = delete;
    MediaDatagramTransmitSession& operator=(
        const MediaDatagramTransmitSession&) = delete;

    static ::media::Result<std::unique_ptr<MediaDatagramTransmitSession>> create(
        const MediaDatagramShapingPlan& plan,
        std::vector<MediaDatagramTransmitEndpointBinding> bindings,
        MediaDatagramTransmitExecutionPlan execution,
        MediaDatagramTransmitPortFactory& portFactory);
    static ::media::Status validateActivation(
        const MediaDatagramShapingPlan& plan,
        const std::vector<MediaDatagramTransmitEndpointBinding>& bindings,
        const MediaDatagramTransmitExecutionPlan& execution);

    MediaDatagramTransmitSubmitResult trySubmitNew(
        std::uint64_t endpointId,
        std::span<const MediaDatagramTransmitJobEntry> entries,
        MediaRunningTime now) noexcept;
    MediaDatagramTransmitSubmitResult retryPending(
        MediaRunningTime now) noexcept;
    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        std::uint64_t endpointId,
        MediaRunningTime now,
        MediaRunningTime maximumWait,
        std::stop_token stopToken) noexcept;
    ::media::Status drainAvailableEvents(MediaRunningTime now) noexcept;
    ::media::Status abort(
        ::media::ErrorInfo cause,
        MediaRunningTime now) noexcept;
    ::media::Status close(MediaRunningTime now) noexcept;

    const MediaDatagramTransmitEvidenceTelemetry& evidenceTelemetry() const noexcept;
    const MediaDatagramTransmitPortCapabilities* capabilities(
        std::uint64_t endpointId) const noexcept;
    std::uint64_t effectiveSocketBytes() const noexcept
    {
        return m_effectiveSocketBytes;
    }
    const std::optional<::media::ErrorInfo>& terminalFailure() const noexcept
    {
        return m_terminalFailure;
    }
    const std::optional<MediaDatagramTransmitError>&
    terminalSubmitFailure() const noexcept
    {
        return m_terminalSubmitFailure;
    }
    bool hasPendingRetry() const noexcept { return m_pendingActive; }

private:
    struct EndpointState final {
        std::uint64_t maximumDatagramBytes;
        std::uint64_t wireOverheadBytes;
        std::unique_ptr<MediaDatagramTransmitPort> port;
        MediaDatagramTransmitPortCapabilities capabilities;
    };

    struct PendingJob final {
        std::uint64_t endpointId;
        std::vector<MediaDatagramTransmitJobEntry> entries;
        std::vector<MediaDatagramTransmitPortRequest> portRequests;
        std::vector<MediaDatagramTransmitEvidenceReservation> reservations;
        bool requiresWritableWait;
    };

    MediaDatagramTransmitSession(std::string sessionKey,
                                 std::string serviceScopeId,
                                 std::uint64_t generation) noexcept;
    MediaDatagramTransmitSubmitResult submitPending(
        MediaRunningTime now) noexcept;
    void clearPending() noexcept;
    ::media::Status advanceClock(MediaRunningTime now) noexcept;
    ::media::Status validateOwnerThread() noexcept;
    ::media::Status terminate(::media::ErrorInfo error) noexcept;
    MediaDatagramTransmitSubmitResult terminateSubmit(
        MediaDatagramTransmitError error) noexcept;
    void closePorts() noexcept;

    std::string m_sessionKey;
    std::string m_serviceScopeId;
    std::uint64_t m_generation;
    std::uint64_t m_burstWireBytes = 0;
    std::uint64_t m_maximumBatchDatagrams = 0;
    std::uint64_t m_maximumBatchBytes = 0;
    std::uint64_t m_effectiveSocketBytes = 0;
    std::unordered_map<std::uint64_t, EndpointState> m_endpoints;
    std::unique_ptr<MediaDatagramTransmitEvidenceCollector> m_evidence;
    std::unique_ptr<PendingJob> m_pending;
    std::vector<std::uint64_t> m_evidenceIdsScratch;
    std::vector<std::optional<std::uint64_t>> m_launchTimesScratch;
    std::optional<MediaRunningTime> m_lastNow;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    std::optional<MediaDatagramTransmitError> m_terminalSubmitFailure;
    std::thread::id m_ownerThread;
    bool m_pendingActive = false;
    bool m_portsClosed = false;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
