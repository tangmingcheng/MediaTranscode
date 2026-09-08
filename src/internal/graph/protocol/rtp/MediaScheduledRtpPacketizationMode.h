#pragma once

namespace media::ffmpeg::graph {

enum class MediaScheduledRtpPacketizationMode {
    H264AnnexB,
    AacLatm,
    HevcAnnexB
};

} // namespace media::ffmpeg::graph
