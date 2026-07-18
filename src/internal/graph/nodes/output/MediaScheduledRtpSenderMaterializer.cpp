#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaH264SdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpSessionIdentityMaterializer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
}

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using ParametersResult =
    ::media::Result<::media::ffmpeg::CodecParametersPtr>;

::media::Result<MediaRtpUdpLocalPortPolicy> cloneLocalPortPolicy(
    const MediaRtpUdpLocalPortPolicy& source)
{
    switch (source.kind()) {
    case MediaRtpUdpLocalPortPolicyKind::FixedAdjacent:
        if (!source.rtpPort() || !source.rtcpPort()) {
            return ::media::Result<MediaRtpUdpLocalPortPolicy>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Fixed RTP sender port policy is incomplete"));
        }
        return MediaRtpUdpLocalPortPolicy::fixedAdjacent(
            *source.rtpPort(), *source.rtcpPort());
    case MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent:
        return ::media::Result<MediaRtpUdpLocalPortPolicy>::success(
            MediaRtpUdpLocalPortPolicy::osAssignedIndependent());
    }
    return ::media::Result<MediaRtpUdpLocalPortPolicy>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Unknown RTP sender local port policy"));
}

::media::Result<MediaRtpUdpSenderConfig> cloneTransportConfig(
    const MediaRtpUdpSenderConfig& source)
{
    auto localPolicy = cloneLocalPortPolicy(source.localPortPolicy());
    if (!localPolicy) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(
            localPolicy.error());
    }
    return MediaRtpUdpSenderConfig::create(
        source.addressFamily(), source.localNumericAddress(),
        source.remoteRtpEndpoint().numericAddress(),
        source.remoteRtpEndpoint().port(),
        source.remoteRtcpEndpoint().port(),
        std::move(localPolicy).value(), source.sendBufferBytes(),
        source.maximumDatagramBytes(), source.ioBehavior());
}

ParametersResult materializeCodecParameters(
    const AVCodecContext& context,
    const MediaScheduledRtpPacketizationPlan& packetization)
{
    const auto expectedType = packetization.streamKind() ==
            MediaStreamKind::Video
        ? AVMEDIA_TYPE_VIDEO
        : AVMEDIA_TYPE_AUDIO;
    const std::string codecName = canonicalCodecName(
        avcodec_get_name(context.codec_id));
    const AVRational expectedTimeBase{
        packetization.streamTimeBaseNumerator(),
        packetization.streamTimeBaseDenominator()};
    if (context.codec_type != expectedType ||
        codecName != packetization.codecName() ||
        av_cmp_q(context.time_base, expectedTimeBase) != 0) {
        return ParametersResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "Runtime encoder metadata does not match planned RTP packetization"));
    }
    if (packetization.streamKind() == MediaStreamKind::Audio) {
        if (!packetization.maximumAccessUnitSamples() ||
            context.sample_rate != packetization.streamTimeBaseDenominator() ||
            context.frame_size != *packetization.maximumAccessUnitSamples()) {
            return ParametersResult::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Runtime audio encoder metadata does not match planned RTP access units"));
        }
    } else if (packetization.maximumAccessUnitSamples()) {
        return ParametersResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "Runtime video RTP packetization rejects audio sample limits"));
    }
    return FFmpegCodecParametersMaterializer::fromContext(context);
}

::media::Result<MediaRtpSdpMediaDescription> materializeMediaDescription(
    const MediaScheduledRtpOutputPlan& plan,
    const AVCodecParameters& parameters)
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
        auto codec = MediaH264SdpCodecDescriptionFactory::create(parameters);
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
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaPlaybackEpoch& playbackEpoch)
{
    auto session = MediaRtpSdpSessionIdentityMaterializer::materialize(
        sdpPlan, sharedNtpEpoch, playbackEpoch.generation);
    if (!session) {
        return ::media::Result<MediaBufferRef>::failure(session.error());
    }
    auto media = materializeMediaDescription(outputPlan, parameters);
    if (!media) {
        return ::media::Result<MediaBufferRef>::failure(media.error());
    }
    return MediaRtpSenderDescriptionBuffer::create(
        outputPlan.stream, playbackEpoch.generation,
        std::move(session).value(), std::move(media).value());
}

::media::Result<ScheduledRtpSenderConfig> materializeSenderConfig(
    const MediaScheduledRtpOutputPlan& outputPlan,
    const AVCodecParameters& parameters,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaPlaybackEpoch& playbackEpoch)
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
        playbackEpoch.masterRelease);
    if (!mapper) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            mapper.error());
    }
    auto firstReport = playbackEpoch.masterRelease.checkedAdd(
        outputPlan.senderReportInterval);
    if (!firstReport) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            firstReport.error());
    }
    auto reportSchedule = MediaRtcpSenderReportSchedule::create(
        firstReport.value(), outputPlan.senderReportInterval,
        outputPlan.senderReportInterval, playbackEpoch.generation);
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
        reportSchedule.value(), outputPlan.cname, playbackEpoch.generation,
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
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaPlaybackEpoch& playbackEpoch)
{
    auto parameters = materializeCodecParameters(
        codecContext, outputPlan.packetization);
    if (!parameters) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            parameters.error());
    }
    auto description = materializeDescription(
        outputPlan, sdpPlan, *parameters.value(), sharedNtpEpoch,
        playbackEpoch);
    if (!description) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            description.error());
    }
    auto senderConfig = materializeSenderConfig(
        outputPlan, *parameters.value(), sharedNtpEpoch, playbackEpoch);
    if (!senderConfig) {
        return ::media::Result<
            MediaScheduledRtpSenderMaterialization>::failure(
            senderConfig.error());
    }
    auto transportConfig = cloneTransportConfig(outputPlan.transport);
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
