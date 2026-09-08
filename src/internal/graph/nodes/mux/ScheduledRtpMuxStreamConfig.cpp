#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"

#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/bsf.h>
#include <libavcodec/codec_par.h>
}

#include <memory>
#include <span>
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

bool isDynamicPayloadType(int payloadType) noexcept
{
    return payloadType >= 96 && payloadType <= 127;
}

bool packetizationMatches(MediaScheduledRtpPacketizationMode mode,
                          const AVCodecParameters& parameters) noexcept
{
    return (mode == MediaScheduledRtpPacketizationMode::H264AnnexB &&
            parameters.codec_type == AVMEDIA_TYPE_VIDEO &&
            parameters.codec_id == AV_CODEC_ID_H264) ||
           (mode == MediaScheduledRtpPacketizationMode::HevcAnnexB &&
            parameters.codec_type == AVMEDIA_TYPE_VIDEO &&
            parameters.codec_id == AV_CODEC_ID_HEVC) ||
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

struct BsfDeleter final {
    void operator()(AVBSFContext* context) const noexcept
    {
        if (context) av_bsf_free(&context);
    }
};

using BsfPtr = std::unique_ptr<AVBSFContext, BsfDeleter>;

::media::Result<::media::ffmpeg::CodecParametersPtr>
materializeCodecParameters(
    const AVCodecParameters& parameters,
    AVRational streamTimeBase,
    MediaScheduledRtpPacketizationMode packetizationMode)
{
    using ParametersResult =
        ::media::Result<::media::ffmpeg::CodecParametersPtr>;
    const AVCodecParameters* source = &parameters;
    BsfPtr normalizer;
    if ((packetizationMode == MediaScheduledRtpPacketizationMode::H264AnnexB ||
         packetizationMode == MediaScheduledRtpPacketizationMode::HevcAnnexB) &&
        parameters.extradata && parameters.extradata_size > 0) {
        const bool hevc = packetizationMode ==
            MediaScheduledRtpPacketizationMode::HevcAnnexB;
        const AVBitStreamFilter* filter = av_bsf_get_by_name(
            hevc ? "hevc_mp4toannexb" : "h264_mp4toannexb");
        if (!filter) {
            return ParametersResult::failure(
                ::media::ErrorInfo::unsupported(
                    std::string("FFmpeg ") + (hevc ? "HEVC" : "H264") +
                    " Annex-B codec configuration normalizer is unavailable"));
        }
        AVBSFContext* raw = nullptr;
        const int allocated = av_bsf_alloc(filter, &raw);
        normalizer.reset(raw);
        if (allocated < 0 || !normalizer) {
            return ParametersResult::failure(
                allocated < 0
                    ? FFmpegGraphError::fromCode(
                          allocated,
                          "av_bsf_alloc(scheduled video Annex-B configuration)")
                    : ::media::ErrorInfo::allocationFailed(
                          "scheduled video Annex-B configuration"));
        }
        const int copied = avcodec_parameters_copy(
            normalizer->par_in, &parameters);
        if (copied < 0) {
            return ParametersResult::failure(
                FFmpegGraphError::fromCode(
                    copied,
                    "avcodec_parameters_copy(scheduled video Annex-B input)"));
        }
        normalizer->time_base_in = streamTimeBase;
        const int initialized = av_bsf_init(normalizer.get());
        if (initialized < 0) {
            return ParametersResult::failure(
                FFmpegGraphError::fromCode(
                    initialized,
                    "av_bsf_init(scheduled video Annex-B configuration)"));
        }
        source = normalizer->par_out;
        if (!source->extradata || source->extradata_size <= 0) {
            return ParametersResult::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video Annex-B configuration normalization produced no parameter sets"));
        }
        auto valid = MediaAnnexBAccessUnitValidator::validate(
            std::span<const std::uint8_t>(
                source->extradata,
                static_cast<std::size_t>(source->extradata_size)),
            hevc ? MediaAnnexBCodec::Hevc : MediaAnnexBCodec::H264);
        if (!valid) return ParametersResult::failure(valid.error());
    }
    auto materialized = ::media::ffmpeg::makeCodecParameters();
    if (!materialized) {
        return ParametersResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled RTP codec parameters"));
    }
    const int copied = avcodec_parameters_copy(
        materialized.get(), source);
    if (copied < 0) {
        return ParametersResult::failure(
            FFmpegGraphError::fromCode(
                copied,
                "avcodec_parameters_copy(scheduled rtp)"));
    }
    return ParametersResult::success(std::move(materialized));
}

} // namespace

ScheduledRtpMuxStreamConfig::ScheduledRtpMuxStreamConfig(
    MediaStreamKind streamKind,
    ::media::ffmpeg::CodecParametersPtr codecParameters,
    AVRational streamTimeBase,
    MediaScheduledRtpPacketizationMode packetizationMode,
    MediaRtpDatagramRewriteIdentity identity,
    FFmpegDatagramWriteAvioConfig avioConfig,
    MediaRtpAccessUnitEmissionContract emissionContract) noexcept
    : m_streamKind(streamKind),
      m_codecParameters(std::move(codecParameters)),
      m_streamTimeBase(streamTimeBase),
      m_packetizationMode(packetizationMode),
      m_identity(identity),
      m_avioConfig(avioConfig),
      m_emissionContract(std::move(emissionContract))
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
    int maximumDatagramBytes,
    MediaRtpAccessUnitEmissionContract emissionContract)
{
    if (!isSupportedStreamKind(streamKind) ||
        codecParameters.codec_type != mediaType(streamKind)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP stream kind must match audio or video codec parameters"));
    }
    if (codecParameters.codec_id == AV_CODEC_ID_NONE ||
        !packetizationMatches(packetizationMode, codecParameters) ||
        !validTimeBase(streamTimeBase) ||
        !isDynamicPayloadType(payloadType)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP stream requires matching packetization, a positive time base, and a dynamic payload type"));
    }
    if (packetizationMode == MediaScheduledRtpPacketizationMode::AacLatm &&
        !isCompleteAacLatmConfig(codecParameters, streamTimeBase)) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AAC LATM requires sample rate, channel layout, ASC, and time base 1/sample_rate"));
    }
    if (emissionContract.maximumAccessUnitPayloadBytes() == 0 ||
        emissionContract.maximumDatagramsPerAccessUnit() == 0 ||
        emissionContract.authority().empty() ||
        emissionContract.packetizationMode() != packetizationMode ||
        emissionContract.maximumDatagramBytes() !=
            static_cast<std::size_t>(maximumDatagramBytes) ||
        ((streamKind == MediaStreamKind::Video) !=
         emissionContract.packetLayout().has_value())) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP stream requires a matching emission contract"));
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
    auto copied = materializeCodecParameters(
        codecParameters, streamTimeBase, packetizationMode);
    if (!copied) {
        return ::media::Result<ScheduledRtpMuxStreamConfig>::failure(
            copied.error());
    }
    return ::media::Result<ScheduledRtpMuxStreamConfig>::success(
        ScheduledRtpMuxStreamConfig(
            streamKind,
            std::move(copied).value(),
            streamTimeBase,
            packetizationMode,
            identity.value(),
            avioConfig.value(),
            std::move(emissionContract)));
}

} // namespace media::ffmpeg::graph
