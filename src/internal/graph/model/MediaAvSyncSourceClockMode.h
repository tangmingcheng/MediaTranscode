#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvSyncSourceClockMode : std::uint8_t {
    RtpSenderReports = 0,
    MpegTsPcr = 1,
    DemuxTimestamps = 2
};

} // namespace media::ffmpeg::graph
