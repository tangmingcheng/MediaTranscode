#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/realtime/MediaScheduledDatagramPacingPlan.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaProjectMpegTsRuntimePlanBuffer;
class MediaMpegTsRtpDatagramSink;
class MediaScheduledDatagramBatchBuffer;

class MediaScheduledDatagramSenderNode final : public FFmpegNodeRuntime {
public:
    ~MediaScheduledDatagramSenderNode() override;
    static ::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>
    create(MediaNodeId nodeId,
           MediaProtocolOutputSessionKey plannedSession,
           MediaTranscodeStreamSet streamSet,
           std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);

    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;

private:
    MediaScheduledDatagramSenderNode(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSession,
        MediaTranscodeStreamSet streamSet,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);

    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Status bindPlan(const MediaProjectMpegTsRuntimePlanBuffer& plan);
    ::media::Status sendBatch(const MediaScheduledDatagramBatchBuffer& batch);
    ::media::Status waitUntil(MediaRunningTime deadline);
    ::media::Result<MediaNodeProcessResult> failTerminal(::media::ErrorInfo error);
    void emitDiagnostics(const char* stage) noexcept;
    void closeSender() noexcept;

    MediaProtocolOutputSessionKey m_plannedSession;
    MediaTranscodeStreamSet m_streamSet;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    std::unique_ptr<MediaMpegTsRtpDatagramSink> m_sink;
    MediaNodeWakeup m_wakeup;
    std::optional<std::uint64_t> m_generation;
    std::optional<MediaScheduledDatagramPacingPlan> m_pacing;
    std::optional<MediaRunningTime> m_previousPlannedCompletion;
    std::uint64_t m_scheduledBatchMaximumBytes = 0;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    MediaRunningTime m_maximumEnqueueLateness =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_maximumWakeOvershoot =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_maximumSendDuration =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_cumulativeWaitDuration =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_cumulativeSendDuration =
        MediaRunningTime::fromNanoseconds(0);
    std::uint64_t m_batches = 0;
    std::uint64_t m_datagrams = 0;
    std::uint64_t m_bytes = 0;
    std::uint64_t m_enqueueDeadlineMisses = 0;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
