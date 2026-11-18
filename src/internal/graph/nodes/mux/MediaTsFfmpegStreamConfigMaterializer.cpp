#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"

#include "internal/graph/nodes/mux/MediaFfmpegAacAscDialectMaterializer.h"

#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/protocol/mpegts/MediaTsVideoConfigParser.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/channel_layout.h>
}

#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {
namespace {

using ByteSpan = std::span<const std::uint8_t>;

::media::Result<ByteSpan> extradata(const AVCodecParameters& parameters)
{
    if (!parameters.extradata || parameters.extradata_size <= 0) {
        return ::media::Result<ByteSpan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg codec parameters require non-empty extradata"));
    }
    return ::media::Result<ByteSpan>::success(ByteSpan(
        parameters.extradata,
        static_cast<std::size_t>(parameters.extradata_size)));
}

} // namespace

::media::Result<MediaTsMaterializedVideoConfig>
MediaTsFfmpegStreamConfigMaterializer::video(
    const MediaTsMuxPlan& plan,
    const AVCodecParameters& parameters)
{
    const auto& contract = plan.parameters().video;
    const AVCodecID expectedCodec = contract.codec() == MediaTsVideoCodec::H264
        ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
    if (parameters.codec_type != AVMEDIA_TYPE_VIDEO ||
        parameters.codec_id != expectedCodec) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video materializer codec parameters conflict with the plan"));
    }
    auto bytes = extradata(parameters);
    if (!bytes) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            bytes.error());
    }
    return MediaTsVideoConfigParser::parse(contract, bytes.value());
}

::media::Result<MediaTsMaterializedAudioConfig>
MediaTsFfmpegStreamConfigMaterializer::audio(
    const MediaTsMuxPlan& plan,
    const AVCodecParameters& parameters)
{
    if (parameters.codec_type != AVMEDIA_TYPE_AUDIO ||
        parameters.codec_id != AV_CODEC_ID_AAC) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS audio materializer requires AAC codec parameters"));
    }
    auto bytes = extradata(parameters);
    if (!bytes) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            bytes.error());
    }
    auto canonicalAsc = MediaFfmpegAacAscDialectMaterializer::canonicalize(
        bytes.value());
    if (!canonicalAsc) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            canonicalAsc.error());
    }
    auto asc = parseMediaAacAudioSpecificConfig(canonicalAsc.value());
    if (!asc) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            asc.error());
    }
    const auto* program = plan.audioVideoProgram();
    if (!program) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS audio materializer rejects a VideoOnly mux plan"));
    }
    const auto& planned = program->aac;
    if (planned.audioObjectType != asc.value().audioObjectType ||
        planned.samplingFrequencyIndex != asc.value().samplingFrequencyIndex ||
        planned.channelConfiguration != asc.value().channelConfiguration ||
        parameters.sample_rate != asc.value().sampleRate ||
        av_channel_layout_check(&parameters.ch_layout) != 1 ||
        parameters.ch_layout.nb_channels != asc.value().channels) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ASC, plan, sample rate, and channel layout conflict"));
    }
    return MediaTsMaterializedAudioConfig::create(
        asc.value().audioObjectType,
        asc.value().samplingFrequencyIndex,
        asc.value().channelConfiguration);
}

} // namespace media::ffmpeg::graph
