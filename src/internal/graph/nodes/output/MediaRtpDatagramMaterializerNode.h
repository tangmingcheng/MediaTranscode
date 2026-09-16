#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationData.h"
#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"
#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <memory>
#include <optional>
#include <vector>
#include <deque>

namespace media::ffmpeg::graph {

struct MediaRtpDatagramMaterializerNodeDependencies final {
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority;
    std::unique_ptr<ScheduledRtpPacketizerFactory> packetizerFactory;
};

struct MediaRtpMaterializerGenerationData final {
    MediaBufferRef activation;
    std::optional<MediaProtocolOutputActivation> activationFacts;
    MediaBufferRef transportPlan;
    MediaBufferRef terminal;
    bool terminalReportPrepared = false;
    MediaBufferRef stagedConfigurationAccessUnit;
    MediaBufferRef pendingAccessUnit;
    MediaBufferRef pendingDescription;
    std::deque<MediaBufferRef> pendingWireOutputs;
    bool descriptionEmitted = false;
    std::unique_ptr<ScheduledRtpPacketizerSession> packetizer;
    std::optional<MediaRtpWireDatagramMaterializer> wireMaterializer;
    std::vector<std::vector<std::uint8_t>> packetizedBytes;
    std::vector<std::size_t> packetizedPayloadOctets;
};

using MediaRtpMaterializerGenerationSession = MediaProtocolOutputGenerationData<MediaRtpMaterializerGenerationData>;

class MediaRtpDatagramMaterializerNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaRtpDatagramMaterializerNode>>
    create(MediaNodeId nodeId,
           MediaProtocolOutputSessionKey plannedSessionKey,
           MediaScheduledRtpOutputPlan outputPlan,
           MediaSeparateRtpSdpRuntimePlan sdpPlan,
           MediaRtpDatagramMaterializerNodeDependencies dependencies);
    static MediaNodeKind staticKind() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_generationPurge; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

    std::string_view generationPurgeIdentity() const noexcept;
    MediaScheduledStream scheduledStream() const noexcept { return m_outputPlan.stream; }
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    MediaRtpDatagramMaterializerNode(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSessionKey,
        MediaScheduledRtpOutputPlan outputPlan,
        MediaSeparateRtpSdpRuntimePlan sdpPlan,
        MediaRtpDatagramMaterializerNodeDependencies dependencies);
    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Result<bool> acquireBindings(
        MediaGraphExecutionContext& context);
    ::media::Status openProtocol(const AVPacket* configurationPacket);
    ::media::Status createWireMaterializer(
        MediaGraphExecutionContext& context,
        std::uint16_t initialSequence);
    ::media::Result<MediaNodeProcessResult> processAccessUnit(
        MediaGraphExecutionContext& context);
    ::media::Status collectPacketizedDatagram(
        std::span<const std::uint8_t> bytes,
        std::size_t payloadOctets);
    ::media::Result<MediaNodeProcessResult> finishProtocol(
        MediaGraphExecutionContext& context);
    ::media::Result<bool> discardPurgedScheduled(MediaBufferRef& buffer);
    void resetState() noexcept;
    std::shared_ptr<MediaRtpMaterializerGenerationSession> m_generationSession = std::make_shared<MediaRtpMaterializerGenerationSession>();
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_generationPurge = std::make_shared<MediaOwnerThreadGenerationPurge>();
    std::optional<MediaAvGenerationPurge> m_completedPurge;

    MediaProtocolOutputSessionKey m_plannedSessionKey;
    MediaScheduledRtpOutputPlan m_outputPlan;
    MediaSeparateRtpSdpRuntimePlan m_sdpPlan;
    MediaRtpDatagramMaterializerNodeDependencies m_dependencies;
    MediaBufferRef m_codec;
};

} // namespace media::ffmpeg::graph
