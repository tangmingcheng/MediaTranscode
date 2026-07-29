#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

inline constexpr std::string_view MediaDemuxVideoClockBinderGenerationIdentity =
    "demux_video_clock_binder";
inline constexpr std::string_view MediaDemuxAudioClockBinderGenerationIdentity =
    "demux_audio_clock_binder";

} // namespace media::ffmpeg::graph
