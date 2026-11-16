#include "internal/graph/planner/capability/MediaEncoderCompletionAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderOpenContractAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppEncoderCompletionAdapter.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

}

#include <cstring>
#include <sstream>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {
std::array<std::int64_t, 22> publicConfiguration(const AVCodecContext& c)
{
    return {c.codec_id, c.codec_type, c.profile, c.width, c.height, c.pix_fmt, c.sw_pix_fmt,
        c.time_base.num, c.time_base.den, c.framerate.num, c.framerate.den,
        c.sample_aspect_ratio.num, c.sample_aspect_ratio.den,
        c.bit_rate, c.rc_max_rate, c.rc_buffer_size, c.gop_size,
        c.max_b_frames, c.has_b_frames, c.flags, c.flags2, c.field_order};
}
std::optional<MediaNvencCompletionConfiguration> nvencConfiguration(const AVCodecContext& c)
{
    MediaNvencCompletionConfiguration facts{};
    if (!c.priv_data || av_opt_get_int(c.priv_data, "delay", 0, &facts.delay) < 0 ||
        av_opt_get_int(c.priv_data, "zerolatency", 0, &facts.zeroLatency) < 0 ||
        av_opt_get_int(c.priv_data, "rc-lookahead", 0, &facts.lookahead) < 0) return {};
    return facts;
}
}

::media::Status MediaEncoderCompletionAdapter::validateUnchanged(
    const AVCodecContext& encoder, const MediaVideoEncoderCompletionContract& prepared)
{
    const bool privateMatch = std::visit([&](const auto& expected) {
        using Configuration = std::decay_t<decltype(expected)>;
        if constexpr (std::is_same_v<Configuration, MediaNvencCompletionConfiguration>) {
            if (prepared.lineagePropagation != MediaVideoLineagePropagation::CodecCopyOpaque) return false;
            const auto actual = nvencConfiguration(encoder);
            return actual && *actual == expected;
        } else {
            if (prepared.lineagePropagation != MediaVideoLineagePropagation::SubmissionOrder) return false;
            const auto actual = MediaRkmppEncoderCompletionAdapter::configuration(encoder);
            return actual && *actual == expected;
        }
    }, prepared.configuration.backend);
    if (!privateMatch || publicConfiguration(encoder) != prepared.configuration.fields)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Encoded progress prepared configuration changed"));
    return ::media::Status::success();
}

namespace {
std::optional<MediaVideoEncoderCompletionContract>
readNvencAfterOpen(const AVCodecContext& encoder)
{
    if (!encoder.codec || !encoder.codec->name || !encoder.priv_data ||
        !avcodec_is_open(const_cast<AVCodecContext*>(&encoder)) ||
        encoder.codec_type != AVMEDIA_TYPE_VIDEO ||
        encoder.codec != avcodec_find_encoder_by_name(encoder.codec->name)) return {};
    const std::string_view name(encoder.codec->name);
    if (!((name == "h264_nvenc" && encoder.codec_id == AV_CODEC_ID_H264) ||
          (name == "hevc_nvenc" && encoder.codec_id == AV_CODEC_ID_HEVC))) return {};

    // Audited nvenc.c writes frameFieldMode from INTERLACED_DCT, writes the
    // final multiview profile back, and sets has_b_frames when frameIntervalP
    // exceeds one. All-intra frameIntervalP=0 is also zero effective reorder.
    if ((encoder.flags & AV_CODEC_FLAG_INTERLACED_DCT) ||
        !(encoder.flags & AV_CODEC_FLAG_COPY_OPAQUE) ||
        encoder.profile == AV_PROFILE_UNKNOWN ||
        (encoder.codec_id == AV_CODEC_ID_HEVC &&
         encoder.profile == AV_PROFILE_HEVC_MULTIVIEW_MAIN) ||
        encoder.gop_size < 0 || encoder.max_b_frames != 0 || encoder.has_b_frames != 0 ||
        encoder.time_base.num <= 0 || encoder.time_base.den <= 0 ||
        encoder.framerate.num <= 0 || encoder.framerate.den <= 0) return {};
    // NVENC publishes initialized maxBitRate/vbvBufferSize here. Positive
    // public values must already agree: reconfig_encoder otherwise can reset
    // on send, and a failed reconfigure is logged rather than propagated.
    // Zero public values do not request a change in the audited implementation.
    if (encoder.nb_coded_side_data <= 0 || !encoder.coded_side_data ||
        encoder.bit_rate <= 0 || encoder.rc_max_rate < 0 || encoder.rc_buffer_size < 0) return {};
    const auto* side = av_packet_side_data_get(encoder.coded_side_data,
        encoder.nb_coded_side_data, AV_PKT_DATA_CPB_PROPERTIES);
    if (!side || !side->data || side->size < sizeof(AVCPBProperties)) return {};
    AVCPBProperties cpb;
    std::memcpy(&cpb, side->data, sizeof(cpb));
    if (cpb.avg_bitrate != encoder.bit_rate ||
        (encoder.rc_max_rate > 0 && cpb.max_bitrate != encoder.rc_max_rate) ||
        (encoder.rc_buffer_size > 0 && cpb.buffer_size != encoder.rc_buffer_size)) return {};

    if (!MediaEncoderOpenContractAdapter::verifyOutputBuffering(
            encoder, MediaEncoderOutputBuffering::Immediate)) return {};
    const auto snapshot = nvencConfiguration(encoder);
    if (!snapshot) return {};

    // Full locked surface -> one AVPacket; zero-delay timestamp FIFO and
    // inputDuration/outputDuration slot transport establish completion/identity.
    // See docs/realtime-composition-encoded-progress-plan.md for the exact
    // dependency revision/hash gate and the required immutable-context lifetime.
    return MediaVideoEncoderCompletionContract{
        {encoder.time_base.num, encoder.time_base.den},
        {encoder.framerate.num, encoder.framerate.den},
        MediaEncoderCompletionLifetime::UnchangedPreparedContextUntilReset,
        MediaVideoLineagePropagation::CodecCopyOpaque,
        {publicConfiguration(encoder), *snapshot},
        "audited-nvenc-full-surface-packet-zero-reorder-opaque-slot"};
}

} // namespace

std::optional<MediaVideoEncoderCompletionContract>
MediaEncoderCompletionAdapter::readAfterOpen(const AVCodecContext& encoder)
{
    std::optional<MediaVideoEncoderCompletionContract> result;
    std::string_view reason = "unrecognized_opened_encoder";
    const std::string_view name = encoder.codec && encoder.codec->name ? encoder.codec->name : "";
    const bool native = encoder.codec && encoder.codec->name && encoder.priv_data &&
        avcodec_is_open(const_cast<AVCodecContext*>(&encoder)) &&
        encoder.codec_type == AVMEDIA_TYPE_VIDEO &&
        encoder.codec == avcodec_find_encoder_by_name(encoder.codec->name);
    const bool rkmpp = (name == "h264_rkmpp" && encoder.codec_id == AV_CODEC_ID_H264) ||
        (name == "hevc_rkmpp" && encoder.codec_id == AV_CODEC_ID_HEVC);
    if (native && rkmpp) {
        result = MediaRkmppEncoderCompletionAdapter::readAfterOpen(
            encoder, publicConfiguration(encoder), reason);
    } else if (native && (name == "h264_nvenc" || name == "hevc_nvenc")) {
        result = readNvencAfterOpen(encoder);
        reason = result ? "authorized" : "nvenc_prepared_contract_unproven";
    }
    // One observation per preparation/readback, never from per-frame validation.
    // A diagnostic failure must not change capability or production errors.
    try {
        if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
            std::ostringstream out;
            out << "encoder_completion_prepared codec=" << name
                << " authorized=" << static_cast<bool>(result) << " reason=" << reason;
            if (result) out << " lineage=" << mediaVideoLineagePropagationName(result->lineagePropagation)
                << " time_base=" << result->inputTimeBase.num << '/' << result->inputTimeBase.den
                << " cadence=" << result->inputCadence.num << '/' << result->inputCadence.den;
            if (rkmpp) {
                const auto facts = MediaRkmppEncoderCompletionAdapter::configuration(encoder);
                if (facts) out << " split_known=" << facts->known << " split_mode=" << facts->splitMode
                    << " split_arg=" << facts->splitArgument << " split_out=" << facts->splitOutput
                    << " low_delay=" << facts->lowDelay << " drop_mode=" << facts->dropMode
                    << " fps_in_flex=" << facts->inputFlexible << " fps_in=" << facts->inputNumerator << '/' << facts->inputDenominator
                    << " fps_out_flex=" << facts->outputFlexible << " fps_out=" << facts->outputNumerator << '/' << facts->outputDenominator;
            }
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::PlannerCapability, out.str());
        }
    } catch (...) {}
    return result;
}
} // namespace media::ffmpeg::graph
