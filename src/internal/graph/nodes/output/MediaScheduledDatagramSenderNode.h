#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/network/MediaDatagramPacingController.h"
#include "internal/graph/runtime/network/MediaDatagramServiceScopeCoordinator.h"
#include "internal/graph/runtime/network/MediaDatagramTransmitSession.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <memory>
#include <optional>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

class MediaDatagramTransportPlanBuffer;
class MediaWireDatagramBatchBuffer;
class MediaWireGlobalSequenceState;

struct MediaScheduledDatagramSenderNodeDependencies final {
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> clock;
    std::unique_ptr<MediaDatagramTransmitPortFactory> portFactory;
};

class MediaScheduledDatagramSenderNode final : public FFmpegNodeRuntime {
public:
    ~MediaScheduledDatagramSenderNode() override;
    static ::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>
    create(MediaNodeId nodeId,
           MediaProtocolOutputSessionKey plannedSession,
           MediaTranscodeStreamSet streamSet,
           MediaScheduledDatagramSenderNodeDependencies dependencies);

    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;

private:
    enum class SubmitState {
        WaitReservation,
        TrySubmit,
        WaitWritableWithinOriginalDeadline
    };

    MediaScheduledDatagramSenderNode(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSession,
        MediaTranscodeStreamSet streamSet,
        MediaScheduledDatagramSenderNodeDependencies dependencies) noexcept;

    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Status bindPlan(const MediaDatagramTransportPlanBuffer& plan);
    ::media::Result<MediaNodeProcessResult> progressPendingBatch();
    ::media::Result<MediaNodeProcessResult> finishAfterEvidenceDrain();
    ::media::Status waitUntil(MediaRunningTime deadline);
    ::media::Status waitUntilSteady(
        std::chrono::steady_clock::time_point deadline);
    ::media::Status reserveServiceScope();
    ::media::Status beginSubmitGroup();
    ::media::Status preflightBatchTelemetry(
        const MediaWireDatagramBatchBuffer& batch) const;
    ::media::Status enqueueWireBatch(
        std::shared_ptr<MediaWireDatagramBatchBuffer> batch);
    ::media::Result<bool> activateNextWireBatch();
    bool allBatchInputsDrained(
        MediaGraphExecutionContext& context) const noexcept;
    ::media::Status recordSubmittedPrefix(
        std::size_t count,
        MediaRunningTime submitCompletedAt);
    ::media::Status settleServiceScopeFailure(
        const MediaDatagramTransmitError& error,
        std::chrono::steady_clock::time_point submitStartedAt,
        std::chrono::steady_clock::time_point submitCompletedAt);
    ::media::Result<MediaNodeProcessResult> failSubmit(
        const MediaDatagramTransmitError& error,
        MediaRunningTime submitStartedAt,
        MediaRunningTime submitCompletedAt);
    ::media::Result<MediaNodeProcessResult> failTerminal(::media::ErrorInfo error);
    void emitDiagnostics(const char* stage) noexcept;
    void closeSender(::media::ErrorInfo cause) noexcept;

    MediaProtocolOutputSessionKey m_plannedSession;
    MediaTranscodeStreamSet m_streamSet;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_clock;
    std::unique_ptr<MediaDatagramTransmitPortFactory> m_portFactory;
    std::unique_ptr<MediaDatagramPacingController> m_pacingController;
    std::unique_ptr<MediaDatagramServiceScopeMembership>
        m_serviceScopeMembership;
    std::optional<MediaDatagramServiceScopeReservation>
        m_serviceScopeReservation;
    std::unique_ptr<MediaDatagramTransmitSession> m_session;
    std::shared_ptr<MediaWireGlobalSequenceState> m_serviceLedger;
    std::shared_ptr<MediaWireDatagramBatchBuffer> m_pendingBatch;
    MediaNodeWakeup m_wakeup;
    std::stop_source m_stopSource;
    std::optional<std::uint64_t> m_generation;
    std::string m_serviceScopeId;
    MediaDatagramTransmitExecutionMode m_executionMode =
        MediaDatagramTransmitExecutionMode::Unknown;
    std::unordered_map<std::uint64_t, std::uint64_t> m_wireOverheadBytes;
    std::unordered_map<std::uint64_t, std::uint64_t> m_endpointDatagrams;
    std::unordered_map<std::uint64_t, std::uint64_t> m_endpointBytes;
    std::vector<std::uint64_t> m_endpointIds;
    std::vector<MediaDatagramTransmitJobEntry> m_submitEntries;
    struct QueuedWireBatch final {
        std::uint64_t firstGlobalSequence;
        std::uint64_t lastGlobalSequence;
        std::uint64_t wireBytes;
        std::shared_ptr<MediaWireDatagramBatchBuffer> batch;
    };
    std::vector<QueuedWireBatch> m_queuedWireBatches;
    std::uint64_t m_burstWireBytes = 0;
    std::uint64_t m_maximumBatchDatagrams = 0;
    std::uint64_t m_maximumBatchBytes = 0;
    std::uint64_t m_maximumBacklogDatagrams = 0;
    std::uint64_t m_maximumBacklogBytes = 0;
    std::uint64_t m_queuedWireDatagrams = 0;
    std::uint64_t m_queuedWireBytes = 0;
    std::uint64_t m_maximumQueuedWireBatches = 0;
    std::uint64_t m_maximumQueuedWireDatagrams = 0;
    std::uint64_t m_maximumQueuedWireBytes = 0;
    std::optional<std::uint64_t> m_nextScheduledSequence;
    SubmitState m_state = SubmitState::WaitReservation;
    std::size_t m_nextDatagram = 0;
    std::size_t m_groupBegin = 0;
    std::size_t m_groupCount = 0;
    std::uint64_t m_groupEndpointId = 0;
    std::uint64_t m_groupWireBytes = 0;
    MediaRunningTime m_groupNotBefore = MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_groupDeadline = MediaRunningTime::fromNanoseconds(0);
    std::optional<::media::ErrorInfo> m_terminalFailure;
    std::uint64_t m_batches = 0;
    std::uint64_t m_datagrams = 0;
    std::uint64_t m_bytes = 0;
    std::uint64_t m_wouldBlockEvents = 0;
    std::uint64_t m_writableWaits = 0;
    std::uint64_t m_deadlineMisses = 0;
    std::uint64_t m_pressureFailures = 0;
    std::uint64_t m_partialSubmittedFailures = 0;
    std::uint64_t m_ambiguousSubmittedFailures = 0;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
