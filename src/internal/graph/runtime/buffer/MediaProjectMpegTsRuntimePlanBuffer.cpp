#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"

#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"

#include <memory>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef>
MediaProjectMpegTsRuntimePlanBuffer::create(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaProtocolOutputActivation activation,
    const MediaDatagramTransportPlanBuffer& transportPlan)
{
    const auto& transport = transportPlan.globalSequence();
    if (!outputPlan || !sessionKey.valid() || !transport ||
        transport->sessionKey() != sessionKey.value() ||
        transport->generation() != activation.generation || !transport->storageResource()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a valid output session"));
    }
    if (activation.generation == 0 ||
        (activation.completedTransitionSequence &&
         *activation.completedTransitionSequence == 0)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a positive epoch generation"));
    }
    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(&outputPlan->transport);
    auto mediaEndpoint = transportPlan.endpointId(rtp
        ? MediaDatagramProtocolEndpointRole::MpegTsRtp
        : MediaDatagramProtocolEndpointRole::MpegTsUdp);
    if (!mediaEndpoint) return ::media::Result<MediaBufferRef>::failure(mediaEndpoint.error());
    std::optional<MediaTsPrefixReportFacts> report;
    if (rtp) {
        auto endpoint = transportPlan.endpointId(MediaDatagramProtocolEndpointRole::MpegTsRtcp);
        if (!endpoint) return ::media::Result<MediaBufferRef>::failure(endpoint.error());
        report.emplace(MediaTsPrefixReportFacts{endpoint.value(), rtp->cname().size(),
            rtp->rtcpReporting().minimumAdmissionInterval()});
    }
    auto service = MediaTsPrefixServicePlan::create(outputPlan->emission,
        transportPlan.plan().shaping, mediaEndpoint.value(), report);
    if (!service) return ::media::Result<MediaBufferRef>::failure(service.error());
    return ::media::Result<MediaBufferRef>::success(
        std::shared_ptr<MediaProjectMpegTsRuntimePlanBuffer>(
            new MediaProjectMpegTsRuntimePlanBuffer(
                std::move(outputPlan), std::move(sessionKey), streamSet,
                activation, transport, std::move(service).value())));
}

MediaProjectMpegTsRuntimePlanBuffer::MediaProjectMpegTsRuntimePlanBuffer(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaProtocolOutputActivation activation,
    std::shared_ptr<MediaWireGlobalSequenceState> transport,
    MediaTsPrefixServicePlan prefixService)
    : m_outputPlan(std::move(outputPlan))
    , m_sessionKey(std::move(sessionKey))
    , m_streamSet(streamSet)
    , m_activation(activation)
    , m_transport(std::move(transport))
    , m_prefixService(std::move(prefixService))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::ProjectMpegTsRuntimePlan);
}

MediaBufferType MediaProjectMpegTsRuntimePlanBuffer::type() const noexcept
{
    return MediaBufferType::ProjectMpegTsRuntimePlan;
}

const MediaTsMuxPlan&
MediaProjectMpegTsRuntimePlanBuffer::muxPlan() const noexcept
{
    return m_outputPlan->protocol.muxPlan();
}

const MediaProjectMpegTsRuntimeOutputPlan&
MediaProjectMpegTsRuntimePlanBuffer::outputPlan() const noexcept
{
    return *m_outputPlan;
}

const std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>&
MediaProjectMpegTsRuntimePlanBuffer::sharedOutputPlan() const noexcept
{
    return m_outputPlan;
}

const MediaProtocolOutputSessionKey&
MediaProjectMpegTsRuntimePlanBuffer::sessionKey() const noexcept
{
    return m_sessionKey;
}

MediaTranscodeStreamSet
MediaProjectMpegTsRuntimePlanBuffer::streamSet() const noexcept
{
    return m_streamSet;
}

const MediaProtocolOutputActivation&
MediaProjectMpegTsRuntimePlanBuffer::activation() const noexcept
{
    return m_activation;
}

} // namespace media::ffmpeg::graph
