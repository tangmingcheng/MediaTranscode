#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvSyncOutputAdapterKind : std::uint8_t {
    ScheduledSeparateRtp = 0,
    ProjectMpegTs = 1
};

} // namespace media::ffmpeg::graph
