#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/codec_par.h>
}

#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isSupportedStreamKind(MediaStreamKind kind) noexcept
{
    return kind == MediaStreamKind::Video || kind == MediaStreamKind::Audio;
}

AVMediaType mediaType(MediaStreamKind kind) noexcept
{
    return kind == MediaStreamKind::Video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
}

bool validTimeBase(AVRational value) noexcept
{
    return value.num > 0 && value.den > 0;
}

bool packetizationMatches(MediaScheduledRtpPacketizationMode mode,
                          const AVCodecParameters& parameters) noexcept
{
    return (mode == MediaScheduledRtpPacketizationMode::H264AnnexB &&
            parameters.codec_type == AVMEDIA_TYPE_VIDEO &&
            parameters.codec_id == AV_CODEC_ID_H264) ||
           (mode == MediaScheduledRtpPacketizationMode::AacLatm &&
            parameters.codec_type == AVMEDIA_TYPE_AUDIO &&
            parameters.codec_id == AV_CODEC_ID_AAC);
}

bool isCompleteAacLatmConfig(const AVCodecParameters& parameters,
                             AVRational streamTimeBase) noexcept
{
    return parameters.sample_rate > 0 &&
           av_channel_layout_check(&parameters.ch_layout) == 1 &&
           parameters.extradata != nullptr &&
           parameters.extradata_size > 0 &&
           streamTimeBase.num == 1 &&
           streamTimeBase.den == parameters.sample_rate;
}

} // namespace

ScheduledRtpMuxStreamConfig::ScheduledRtpMuxStreamConfig(
    MediaStreamKind streamKind,
    ::media::ffmpeg::CodecParametersPtr codecParameters,
    AVRational streamTimeBase,
    MediaScheduledRtpPacketizationMode packetizationMode,
    MediaRtpDatagramRewriteIdentity identity,
    FFmpegDatagramWriteAvioConfig avioConfig) noexcept
    : m_streamKind(streamKind),
      m_codecParameters(std::move(codecParameters)),
      m_streamTimeBase(streamTimeBase),
      m_packetizationMode(packetizationMode),
      m_identity(identity),
      m_avioConfig(avioConfig)
{
}

::media::Result<ScheduledRtpMuxStreamConfig>
ScheduledRtpMuxStreamConfig::create(
    MediaStreamKind streamKind,
    const AVCodecParameters& codecParameters,
    AVRational streamTimeBase,
    MediaScheduledRtpPacketizationMode packetizationMode,
    int payloadType,
    std::uint32_t ssrc,
    int maximumDatagramBytes)
{
    if (!isSupportedStreamKind(streamKind) ||
        codecParameters.codec_type != mediaType(streamKind)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP stream kind must match audio or video codec parameters"));
    }
    if (codecParameters.codec_id == AV_CODEC_ID_NONE ||
        !packetizationMatches(packetizationMode, codecParameters) ||
        !validTimeBase(streamTimeBase)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP stream requires matching packetization and a positive time base"));
    }
    if (packetizationMode == MediaScheduledRtpPacketizationMode::AacLatm &&
        !isCompleteAacLatmConfig(codecParameters, streamTimeBase)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AAC LATM requires sample rate, channel layout, ASC, and time base 1/sample_rate"));
    }
    auto identity = MediaRtpDatagramRewriteIdentity::create(payloadType, ssrc);
    if (!identity) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            identity.error());
    }
    auto avioConfig = FFmpegDatagramWriteAvioConfig::create(
        maximumDatagramBytes);
    if (!avioConfig) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            avioConfig.error());
    }
    auto copied = ::media::ffmpeg::makeCodecParameters();
    if (!copied) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::allocationFailed("scheduled RTP codec parameters"));
    }
    const int copyResult = avcodec_parameters_copy(copied.get(), &codecParameters);
    if (copyResult < 0) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            FFmpegGraphError::fromCode(
                copyResult, "avcodec_parameters_copy(scheduled rtp)"));
    }
    return ::media::Result<ScheduledRtpMuxStreamConfig>::success(
        ScheduledRtpMuxStreamConfig(
            streamKind,
            std::move(copied),
            streamTimeBase,
            packetizationMode,
            identity.value(),
            avioConfig.value()));
}

} // namespace media::ffmpeg::graph
