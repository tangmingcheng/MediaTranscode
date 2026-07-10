#include "internal/graph/nodes/mux/RtpMuxProtocolIo.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg::graph {

::media::Status RtpMuxProtocolIo::writeHeader(AVFormatContext& context)
{
    const int result = avformat_write_header(&context, nullptr);
    return result < 0 ? FFmpegGraphError::statusFromCode(result, "avformat_write_header(rtp)")
                      : ::media::Status::success();
}

::media::Status RtpMuxProtocolIo::writePacket(AVFormatContext& context, AVPacket& packet)
{
    const int result = av_interleaved_write_frame(&context, &packet);
    return result < 0 ? FFmpegGraphError::statusFromCode(result, "av_interleaved_write_frame(rtp)")
                      : ::media::Status::success();
}

::media::Status RtpMuxProtocolIo::writeTrailer(AVFormatContext& context)
{
    const int result = av_write_trailer(&context);
    return result < 0 ? FFmpegGraphError::statusFromCode(result, "av_write_trailer(rtp)")
                      : ::media::Status::success();
}

} // namespace media::ffmpeg::graph
