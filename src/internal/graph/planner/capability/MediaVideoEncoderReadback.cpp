#include "internal/graph/planner/capability/MediaVideoEncoderReadback.h"
#include "internal/graph/planner/capability/MediaEncoderRandomAccessAdapter.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {
::media::Result<MediaVideoEncoderReadback> MediaVideoEncoderReadback::capture(AVCodecContext& c)
{
    using Result = ::media::Result<MediaVideoEncoderReadback>;
    if (!c.codec || !avcodec_is_open(&c) || c.extradata_size < 0 ||
        (c.extradata_size > 0 && !c.extradata)) return Result::failure(
        ::media::ErrorInfo::notInitialized("encoder readback requires an opened unpublished encoder"));
    auto randomAccess = MediaEncoderRandomAccessAdapter::readAfterOpen(c);
    if (!randomAccess) return Result::failure(randomAccess.error());
    MediaVideoEncoderReadback result;
    result.randomAccess = std::move(randomAccess).value();
    result.fields = {
        {"codec_id", c.codec_id},
        {"codec_type", c.codec_type},
        {"codec_tag", c.codec_tag},
        {"profile", c.profile},
        {"level", c.level},
        {"width", c.width},
        {"height", c.height},
        {"coded_width", c.coded_width},
        {"coded_height", c.coded_height},
        {"pix_fmt", c.pix_fmt},
        {"sw_pix_fmt", c.sw_pix_fmt},
        {"time_base.num", c.time_base.num},
        {"time_base.den", c.time_base.den},
        {"framerate.num", c.framerate.num},
        {"framerate.den", c.framerate.den},
        {"sample_aspect_ratio.num", c.sample_aspect_ratio.num},
        {"sample_aspect_ratio.den", c.sample_aspect_ratio.den},
        {"color_range", c.color_range},
        {"color_primaries", c.color_primaries},
        {"color_trc", c.color_trc},
        {"colorspace", c.colorspace},
        {"chroma_sample_location", c.chroma_sample_location},
        {"field_order", c.field_order},
        {"bit_rate", c.bit_rate},
        {"rc_min_rate", c.rc_min_rate},
        {"rc_max_rate", c.rc_max_rate},
        {"rc_buffer_size", c.rc_buffer_size},
        {"rc_initial_buffer_occupancy", c.rc_initial_buffer_occupancy},
        {"gop_size", c.gop_size},
        {"max_b_frames", c.max_b_frames},
        {"has_b_frames", c.has_b_frames},
        {"delay", c.delay},
        {"flags", c.flags},
        {"flags2", c.flags2},
        {"thread_count", c.thread_count},
        {"thread_type", c.thread_type},
        {"active_thread_type", c.active_thread_type},
        {"compression_level", c.compression_level},
        {"global_quality", c.global_quality},
        {"qmin", c.qmin},
        {"qmax", c.qmax},
        {"refs", c.refs},
        {"bits_per_raw_sample", c.bits_per_raw_sample},
        {"bits_per_coded_sample", c.bits_per_coded_sample}};
    if (c.extradata_size > 0) result.extraData.assign(c.extradata, c.extradata + c.extradata_size);
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
