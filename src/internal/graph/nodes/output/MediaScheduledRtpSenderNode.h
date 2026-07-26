#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderSession.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <memory>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget;
class MediaProtocolOutputGenerationState;

struct MediaScheduledRtpSenderNodeDependencies final {
    std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup;
    std::unique_ptr<MediaUdpDatagramSenderPortFactory> transportFactory;
    std::unique_ptr<ScheduledRtpPacketizerFactory> packetizerFactory;
};

class MediaScheduledRtpSenderNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaScheduledRtpSenderNode>> create(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey plannedGroupKey,
        MediaScheduledRtpOutputPlan outputPlan,
        MediaSeparateRtpSdpRuntimePlan sdpPlan,
        MediaScheduledRtpSenderNodeDependencies dependencies);

    static MediaNodeKind staticKind() noexcept;
    std::string_view generationPurgeIdentity() const noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    MediaScheduledRtpSenderNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey plannedGroupKey,
        MediaScheduledRtpOutputPlan outputPlan,
        MediaSeparateRtpSdpRuntimePlan sdpPlan,
        MediaScheduledRtpSenderNodeDependencies dependencies);

    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Result<bool> acquireActivation(MediaGraphExecutionContext& context);
    ::media::Result<bool> acquireCodec(MediaGraphExecutionContext& context);
    ::media::Status openSender();
    ::media::Result<MediaNodeProcessResult> emitDescription(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> processScheduledInput(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> failTerminal(::media::ErrorInfo error);
    void closeSession() noexcept;
    void resetGenerationState() noexcept;

    MediaAvSyncGroupKey m_plannedGroupKey;
    MediaScheduledRtpOutputPlan m_outputPlan;
    MediaSeparateRtpSdpRuntimePlan m_sdpPlan;
    MediaScheduledRtpSenderNodeDependencies m_dependencies;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    MediaBufferRef m_activation;
    MediaBufferRef m_codec;
    MediaBufferRef m_description;
    std::unique_ptr<MediaRtpUdpSenderTransport> m_transport;
    std::unique_ptr<ScheduledRtpSenderSession> m_sender;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    bool m_descriptionEmitted = false;
};

} // namespace media::ffmpeg::graph
