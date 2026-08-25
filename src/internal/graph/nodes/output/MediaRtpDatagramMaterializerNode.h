#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
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

class MediaRtpDatagramMaterializerNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaRtpDatagramMaterializerNode>>
    create(MediaNodeId nodeId,
           MediaProtocolOutputSessionKey plannedSessionKey,
           MediaScheduledRtpOutputPlan outputPlan,
           MediaSeparateRtpSdpRuntimePlan sdpPlan,
           MediaRtpDatagramMaterializerNodeDependencies dependencies);
    static MediaNodeKind staticKind() noexcept;

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
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
    ::media::Status createWireMaterializer(std::uint16_t initialSequence);
    ::media::Result<MediaNodeProcessResult> processAccessUnit(
        MediaGraphExecutionContext& context);
    ::media::Status collectPacketizedDatagram(
        std::span<const std::uint8_t> bytes,
        std::size_t payloadOctets);
    void resetState() noexcept;

    MediaProtocolOutputSessionKey m_plannedSessionKey;
    MediaScheduledRtpOutputPlan m_outputPlan;
    MediaSeparateRtpSdpRuntimePlan m_sdpPlan;
    MediaRtpDatagramMaterializerNodeDependencies m_dependencies;
    MediaBufferRef m_activation;
    std::optional<MediaProtocolOutputActivation> m_activationFacts;
    MediaBufferRef m_codec;
    MediaBufferRef m_transportPlan;
    MediaBufferRef m_stagedConfigurationAccessUnit;
    MediaBufferRef m_pendingAccessUnit;
    MediaBufferRef m_pendingDescription;
    std::deque<MediaBufferRef> m_pendingWireOutputs;
    bool m_descriptionEmitted = false;
    std::unique_ptr<ScheduledRtpPacketizerSession> m_packetizer;
    std::optional<MediaRtpWireDatagramMaterializer> m_wireMaterializer;
    std::vector<std::vector<std::uint8_t>> m_packetizedBytes;
    std::vector<std::size_t> m_packetizedPayloadOctets;
};

} // namespace media::ffmpeg::graph
