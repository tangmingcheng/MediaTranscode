#include "internal/graph/protocol/codec/MediaVideoParameterSetFacts.h"
#include "internal/graph/protocol/codec/MediaH264SpsCodedSizeParser.h"
#include "internal/graph/protocol/codec/MediaHevcSpsCodedSizeParser.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
}
#include <memory>

namespace media::ffmpeg::graph {
std::optional<MediaVideoColorRangeFact> MediaVideoParameterSetFacts::fromAccessUnit(
    std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
    const MediaEncodedPacketLayout& layout)
{
    const auto units = MediaVideoNalUnitScanner::scan(bytes, codec, layout);
    if (!units) return std::nullopt;
    std::optional<MediaVideoColorRangeFact> result;
    for (auto unit : units.value()) {
        const auto type = codec == MediaAnnexBCodec::H264 ? unit[0] & 0x1fU : (unit[0] >> 1U) & 0x3fU;
        if (codec == MediaAnnexBCodec::H264 && (type == 13U || type == 15U)) return std::nullopt;
        if (type != (codec == MediaAnnexBCodec::H264 ? 7U : 33U)) continue;
        // Annex-B trailing_zero_8bits are outside the SPS RBSP.
        if (layout.kind() == MediaEncodedPacketLayoutKind::StartCodeDelimited)
            while (!unit.empty() && unit.back() == 0) unit = unit.first(unit.size() - 1);
        std::optional<MediaVideoColorRangeFact> fact;
        const auto parsed = codec == MediaAnnexBCodec::H264
            ? MediaH264SpsCodedSizeParser::parse(unit, &fact)
            : MediaHevcSpsCodedSizeParser::parse(unit, &fact);
        if (!parsed || !fact || (result && result->range != fact->range)) return std::nullopt;
        // All possible SPS choices must agree; do not guess the active SPS.
        if (result) fact->signaled = fact->signaled && result->signaled;
        result = fact;
    }
    return result;
}

std::optional<MediaVideoColorRangeFact> MediaVideoParameterSetFacts::fromExtradata(const AVCodecContext& context)
{
    if ((context.codec_id != AV_CODEC_ID_H264 && context.codec_id != AV_CODEC_ID_HEVC) ||
        !context.extradata || context.extradata_size <= 0) return std::nullopt;
    const auto codec = context.codec_id == AV_CODEC_ID_H264 ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
    const auto layout = MediaEncodedPacketLayout::startCodeDelimited();
    const auto bytes = std::span<const std::uint8_t>(context.extradata, static_cast<std::size_t>(context.extradata_size));
    if (bytes[0] != 1) return fromAccessUnit(bytes, codec, layout);

    // Reuse FFmpeg's public configuration-record parser, rather than duplicating
    // avcC/hvcC array and extension handling. Init converts par_out extradata only;
    // no packet is submitted and the prepared encoder is untouched.
    const auto* filter = av_bsf_get_by_name(codec == MediaAnnexBCodec::H264 ? "h264_mp4toannexb" : "hevc_mp4toannexb");
    AVBSFContext* raw = nullptr;
    if (!filter || av_bsf_alloc(filter, &raw) < 0) return std::nullopt;
    const auto release = [](AVBSFContext* value) { av_bsf_free(&value); };
    std::unique_ptr<AVBSFContext, decltype(release)> bsf(raw, release);
    if (avcodec_parameters_from_context(bsf->par_in, &context) < 0 || av_bsf_init(bsf.get()) < 0 ||
        !bsf->par_out->extradata || bsf->par_out->extradata_size <= 0) return std::nullopt;
    return fromAccessUnit(std::span<const std::uint8_t>(bsf->par_out->extradata,
        static_cast<std::size_t>(bsf->par_out->extradata_size)), codec, layout);
}
} // namespace media::ffmpeg::graph
