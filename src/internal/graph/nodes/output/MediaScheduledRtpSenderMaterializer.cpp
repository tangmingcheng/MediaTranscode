#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/nodes/output/MediaScheduledRtpCodecParametersMaterializer.h"
#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaH264SdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaHevcSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpSessionIdentityMaterializer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
}

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaRtpSdpMediaDescription> materializeMediaDescription(
    const MediaScheduledRtpOutputPlan& plan,
    const AVCodecParameters& parameters,
    std::span<const std::uint8_t> codecConfigurationAccessUnit)
{
    const bool video = plan.stream == MediaScheduledStream::Video;
    const int channels = video ? 0 : parameters.ch_layout.nb_channels;
    auto identity = MediaRtpSdpMediaIdentity::create(
        video ? MediaSdpMediaKind::Video : MediaSdpMediaKind::Audio,
        plan.transport.addressFamily(),
        plan.transport.remoteRtpEndpoint().numericAddress(),
        plan.transport.remoteRtcpEndpoint().numericAddress(),
        plan.transport.remoteRtpEndpoint().port(),
        plan.transport.remoteRtcpEndpoint().port(),
        static_cast<std::uint8_t>(plan.packetization.payloadType()),
        plan.ssrc, plan.clockRate, channels);
    if (!identity) {
        return ::media::Result<MediaRtpSdpMediaDescription>::failure(
            identity.error());
    }
    if (video) {
        if (plan.packetization.packetizationMode() ==
            MediaScheduledRtpPacketizationMode::H264AnnexB) {
            auto codec = MediaH264SdpCodecDescriptionFactory::create(
                parameters, codecConfigurationAccessUnit);
            if (!codec) {
                return ::media::Result<MediaRtpSdpMediaDescription>::failure(
                    codec.error());
            }
            return MediaRtpSdpMediaDescription::create(
                std::move(identity).value(), std::move(codec).value());
        }
        if (plan.packetization.packetizationMode() !=
            MediaScheduledRtpPacketizationMode::HevcAnnexB) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Scheduled video RTP has an unsupported packetization mode"));
        }
        auto codec = MediaHevcSdpCodecDescriptionFactory::create(
            parameters, codecConfigurationAccessUnit);
        if (!codec) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(
                codec.error());
        }
        return MediaRtpSdpMediaDescription::create(
            std::move(identity).value(), std::move(codec).value());
    }
    auto codec = MediaAacLatmSdpCodecDescriptionFactory::create(parameters);
    if (!codec) {
        return ::media::Result<MediaRtpSdpMediaDescription>::failure(
            codec.error());
    }
    return MediaRtpSdpMediaDescription::create(
        std::move(identity).value(), std::move(codec).value());
}

::media::Result<MediaBufferRef> materializeDescription(
    const MediaScheduledRtpOutputPlan& outputPlan,
    const MediaSeparateRtpSdpRuntimePlan& sdpPlan,
    const AVCodecParameters& parameters,
    std::span<const std::uint8_t> codecConfigurationAccessUnit,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaProtocolOutputActivation& activation)
{
    auto session = MediaRtpSdpSessionIdentityMaterializer::materialize(
        sdpPlan, sharedNtpEpoch, activation.generation);
    if (!session) {
        return ::media::Result<MediaBufferRef>::failure(session.error());
    }
    auto media = materializeMediaDescription(
        outputPlan, parameters, codecConfigurationAccessUnit);
    if (!media) {
        return ::media::Result<MediaBufferRef>::failure(media.error());
    }
    return MediaRtpSenderDescriptionBuffer::create(
        outputPlan.stream, activation.generation,
        std::move(session).value(), std::move(media).value());
}

::media::Result<ScheduledRtpSenderConfig> materializeSenderConfig(
    const MediaScheduledRtpOutputPlan& outputPlan,
    const AVCodecParameters& parameters,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaProtocolOutputActivation& activation)
{
    const std::size_t maximumDatagramBytes =
        outputPlan.packetization.maximumDatagramBytes();
    if (maximumDatagramBytes >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Planned RTP datagram size exceeds FFmpeg integer range"));
    }
    auto streamConfig = ScheduledRtpMuxStreamConfig::create(
        outputPlan.packetization.streamKind(), parameters,
        AVRational{
            outputPlan.packetization.streamTimeBaseNumerator(),
            outputPlan.packetization.streamTimeBaseDenominator()},
        outputPlan.packetization.packetizationMode(),
        outputPlan.packetization.payloadType(), outputPlan.ssrc,
        static_cast<int>(maximumDatagramBytes));
    if (!streamConfig) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            streamConfig.error());
    }
    auto mapper = MediaRtpOutputClockMapper::create(
        outputPlan.clockRate, outputPlan.baseTimestamp,
        activation.masterRelease);
    if (!mapper) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            mapper.error());
    }
    auto firstReport = activation.masterRelease.checkedAdd(
        outputPlan.senderReportInterval);
    if (!firstReport) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            firstReport.error());
    }
    auto reportSchedule = MediaRtcpSenderReportSchedule::create(
        firstReport.value(), outputPlan.senderReportInterval,
        outputPlan.senderReportInterval, activation.generation);
    if (!reportSchedule) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            reportSchedule.error());
    }
    auto counters = ScheduledRtpSenderCounters::create(0, 0);
    if (!counters) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            counters.error());
    }
    return ScheduledRtpSenderConfig::create(
        std::move(streamConfig).value(), sharedNtpEpoch, mapper.value(),
        reportSchedule.value(), outputPlan.cname, activation.generation,
        counters.value());
}

} // namespace

MediaScheduledRtpSenderMaterialization::
MediaScheduledRtpSenderMaterialization(
    MediaRtpUdpSenderConfig transportConfig,
    ScheduledRtpSenderConfig senderConfig,
    MediaBufferRef description) noexcept
    : m_transportConfig(std::move(transportConfig)),
      m_senderConfig(std::move(senderConfig)),
      m_description(std::move(description))
{
}

MediaRtpUdpSenderConfig
MediaScheduledRtpSenderMaterialization::releaseTransportConfig() noexcept
{
    return std::move(m_transportConfig);
}

ScheduledRtpSenderConfig
MediaScheduledRtpSenderMaterialization::releaseSenderConfig() noexcept
{
    return std::move(m_senderConfig);
}

MediaBufferRef
MediaScheduledRtpSenderMaterialization::releaseDescription() noexcept
{
    return std::move(m_description);
}

::media::Result<MediaScheduledRtpSenderMaterialization>
MediaScheduledRtpSenderMaterializer::materialize(
    const MediaScheduledRtpOutputPlan& outputPlan,
    const MediaSeparateRtpSdpRuntimePlan& sdpPlan,
    const AVCodecContext& codecContext,
    const AVPacket* codecConfigurationAccessUnit,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaProtocolOutputActivation& activation)
{
    auto parameters = MediaScheduledRtpCodecParametersMaterializer::materialize(
        codecContext, outputPlan.packetization);
    if (!parameters) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            parameters.error());
    }
    const std::span<const std::uint8_t> configurationBytes =
        codecConfigurationAccessUnit && codecConfigurationAccessUnit->data &&
            codecConfigurationAccessUnit->size > 0
        ? std::span<const std::uint8_t>(
              codecConfigurationAccessUnit->data,
              static_cast<std::size_t>(codecConfigurationAccessUnit->size))
        : std::span<const std::uint8_t>();
    auto description = materializeDescription(
        outputPlan, sdpPlan, *parameters.value(), configurationBytes, sharedNtpEpoch,
        activation);
    if (!description) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            description.error());
    }
    auto senderConfig = materializeSenderConfig(
        outputPlan, *parameters.value(), sharedNtpEpoch, activation);
    if (!senderConfig) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            senderConfig.error());
    }
    auto transportConfig = outputPlan.transport.clone();
    if (!transportConfig) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            transportConfig.error());
    }
    return ::media::Result<
        MediaScheduledRtpSenderMaterialization>::success(
        MediaScheduledRtpSenderMaterialization(
            std::move(transportConfig).value(),
            std::move(senderConfig).value(),
            std::move(description).value()));
}

} // namespace media::ffmpeg::graph
