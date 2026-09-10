#include "internal/graph/nodes/output/MediaScheduledRtpCodecParametersMaterializer.h"

#include "internal/graph/nodes/mux/MediaFfmpegAacAscDialectMaterializer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <cstddef>
#include <span>

namespace media::ffmpeg::graph {
namespace {

using ParametersResult =
    ::media::Result<::media::ffmpeg::CodecParametersPtr>;

::media::Status validateEncoderMetadata(
    const AVCodecParameters& context,
    MediaRational timeBase,
    const MediaScheduledRtpPacketizationPlan& packetization)
{
    const AVMediaType expectedType =
        packetization.streamKind() == MediaStreamKind::Video
        ? AVMEDIA_TYPE_VIDEO
        : AVMEDIA_TYPE_AUDIO;
    if (context.codec_type != expectedType ||
        canonicalCodecName(avcodec_get_name(context.codec_id)) !=
            packetization.codecName() ||
        timeBase.num <= 0 || timeBase.den <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Runtime encoder metadata does not match planned RTP codec"));
    }
    if (packetization.streamKind() == MediaStreamKind::Audio) {
        if (!packetization.maximumAccessUnitSamples() ||
            context.sample_rate !=
                packetization.streamTimeBaseDenominator() ||
            context.frame_size !=
                *packetization.maximumAccessUnitSamples()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Runtime audio encoder metadata does not match planned RTP access units"));
        }
    } else if (packetization.maximumAccessUnitSamples()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Runtime video RTP codec rejects audio access-unit limits"));
    }
    return ::media::Status::success();
}

::media::Status canonicalizeAacAsc(AVCodecParameters& parameters)
{
    if (!parameters.extradata || parameters.extradata_size <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP AAC encoder metadata has no AudioSpecificConfig"));
    }
    auto canonical = MediaFfmpegAacAscDialectMaterializer::canonicalize(
        std::span<const std::uint8_t>(
            parameters.extradata,
            static_cast<std::size_t>(parameters.extradata_size)));
    if (!canonical) return ::media::Status::failure(canonical.error());

    auto* replacement = static_cast<std::uint8_t*>(
        av_mallocz(canonical.value().size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!replacement) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "Scheduled RTP canonical AAC AudioSpecificConfig"));
    }
    std::copy(canonical.value().begin(), canonical.value().end(), replacement);
    av_freep(&parameters.extradata);
    parameters.extradata = replacement;
    parameters.extradata_size = static_cast<int>(canonical.value().size());
    return ::media::Status::success();
}

} // namespace

ParametersResult MediaScheduledRtpCodecParametersMaterializer::materialize(
    const AVCodecParameters& context,
    MediaRational timeBase,
    const MediaScheduledRtpPacketizationPlan& packetization)
{
    if (auto valid = validateEncoderMetadata(context, timeBase, packetization); !valid) {
        return ParametersResult::failure(valid.error());
    }
    auto copied = ::media::ffmpeg::makeCodecParameters();
    if (!copied) return ParametersResult::failure(::media::ErrorInfo::allocationFailed("RTP codec parameter snapshot"));
    const int copyResult = avcodec_parameters_copy(copied.get(), &context);
    if (copyResult < 0) return ParametersResult::failure(FFmpegGraphError::fromCode(copyResult, "avcodec_parameters_copy(RTP snapshot)"));
    auto parameters = ParametersResult::success(std::move(copied));
    if (packetization.streamKind() == MediaStreamKind::Audio) {
        if (auto canonical = canonicalizeAacAsc(*parameters.value());
            !canonical) {
            return ParametersResult::failure(canonical.error());
        }
    }
    return parameters;
}

} // namespace media::ffmpeg::graph
