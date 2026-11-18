#pragma once

#include "media_transcode/Result.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

// FFmpeg's fftools/ffmpeg_dec.c output boundary publishes decoder pkt_timebase
// for decoded video timestamps. An explicitly conflicting source frame is an
// error; an unset AVFrame.time_base does not invent a different timestamp.
inline ::media::Result<AVRational> resolveMediaDecodedVideoTimeBase(
    const AVCodecContext* context, const AVFrame& frame, bool requireDeclaredAgreement)
{
    using R = ::media::Result<AVRational>;
    if (!context || context->pkt_timebase.num <= 0 || context->pkt_timebase.den <= 0)
        return R::failure(::media::ErrorInfo::notInitialized("Video decoder requires packet time base"));
    if (requireDeclaredAgreement && frame.time_base.num > 0 && frame.time_base.den > 0 &&
        av_cmp_q(frame.time_base, context->pkt_timebase) != 0)
        return R::failure(::media::ErrorInfo::invalidArgument(
            "Decoded source frame time base differs from its prepared packet time base"));
    return R::success(context->pkt_timebase);
}

} // namespace media::ffmpeg::graph
