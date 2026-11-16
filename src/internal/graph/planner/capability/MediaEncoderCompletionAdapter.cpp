#include "internal/graph/planner/capability/MediaEncoderCompletionAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderOpenContractAdapter.h"

extern "C" {
#include <libavcodec/avcodec.h>

}


#include <cstring>


namespace media::ffmpeg::graph {

std::optional<MediaVideoEncoderCompletionContract>
MediaEncoderCompletionAdapter::readAfterOpen(const AVCodecContext& encoder)
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

    // Full locked surface -> one AVPacket; zero-delay timestamp FIFO and
    // inputDuration/outputDuration slot transport establish completion/identity.
    // See docs/realtime-composition-encoded-progress-plan.md for the exact
    // dependency revision/hash gate and the required immutable-context lifetime.
    return MediaVideoEncoderCompletionContract{
        {encoder.time_base.num, encoder.time_base.den},
        {encoder.framerate.num, encoder.framerate.den},
        MediaEncoderCompletionLifetime::UnchangedPreparedContextUntilReset,
        "audited-nvenc-full-surface-packet-zero-reorder-opaque-slot"};
}

} // namespace media::ffmpeg::graph
