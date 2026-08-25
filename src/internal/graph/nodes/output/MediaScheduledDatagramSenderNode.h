#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/network/MediaDatagramTransmitSession.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <memory>
#include <optional>
#include <stop_token>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaDatagramTransportPlanBuffer;
class MediaScheduledWireDatagramBatchBuffer;

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
        WaitWritableWithinOriginalDeadline,
        CommitSubmittedPrefixLeases
    };

    MediaScheduledDatagramSenderNode(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSession,
        MediaTranscodeStreamSet streamSet,
        MediaScheduledDatagramSenderNodeDependencies dependencies) noexcept;

    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Status bindPlan(const MediaDatagramTransportPlanBuffer& plan);
    ::media::Result<MediaNodeProcessResult> progressPendingBatch();
    ::media::Status waitUntil(MediaRunningTime deadline);
    ::media::Status beginSubmitGroup();
    ::media::Status commitSubmittedPrefix(std::size_t count) noexcept;
    ::media::Result<MediaNodeProcessResult> failSubmit(
        const MediaDatagramTransmitError& error);
    ::media::Result<MediaNodeProcessResult> failTerminal(::media::ErrorInfo error);
    void emitDiagnostics(const char* stage) noexcept;
    void closeSender(::media::ErrorInfo cause) noexcept;

    MediaProtocolOutputSessionKey m_plannedSession;
    MediaTranscodeStreamSet m_streamSet;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_clock;
    std::unique_ptr<MediaDatagramTransmitPortFactory> m_portFactory;
    std::unique_ptr<MediaDatagramTransmitSession> m_session;
    std::shared_ptr<MediaScheduledWireDatagramBatchBuffer> m_pendingBatch;
    MediaNodeWakeup m_wakeup;
    std::stop_source m_stopSource;
    std::optional<std::uint64_t> m_generation;
    std::string m_serviceScopeId;
    MediaDatagramTransmitExecutionMode m_executionMode =
        MediaDatagramTransmitExecutionMode::UserspaceNonblocking;
    std::unordered_map<std::uint64_t, std::uint64_t> m_wireOverheadBytes;
    std::uint64_t m_burstWireBytes = 0;
    std::uint64_t m_maximumBatchDatagrams = 0;
    std::uint64_t m_maximumBatchBytes = 0;
    SubmitState m_state = SubmitState::WaitReservation;
    std::size_t m_nextDatagram = 0;
    std::size_t m_groupBegin = 0;
    std::size_t m_groupCount = 0;
    std::uint64_t m_groupEndpointId = 0;
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
