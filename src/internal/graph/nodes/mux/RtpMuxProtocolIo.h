#pragma once

#include <media_transcode/Result.h>

struct AVFormatContext;
struct AVPacket;

namespace media::ffmpeg::graph {

class RtpMuxProtocolIo final {
public:
    static ::media::Status writeHeader(AVFormatContext& context);
    static ::media::Status writePacket(AVFormatContext& context, AVPacket& packet);
    static ::media::Status writeTrailer(AVFormatContext& context);

private:
    RtpMuxProtocolIo() = delete;
};

} // namespace media::ffmpeg::graph
