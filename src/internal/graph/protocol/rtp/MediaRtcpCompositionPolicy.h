#pragma once

namespace media::ffmpeg::graph {

enum class MediaRtcpCompositionMode {
    StrictCompoundRfc3550
};

struct MediaRtcpCompoundPolicy final {
    MediaRtcpCompositionMode mode;
    bool requireCname;
};

} // namespace media::ffmpeg::graph
