#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

inline constexpr int MaximumOpusRtpAccessUnitSamples = 5'760;

::media::Status validateOpusRtpMappingFamilyZeroChannels(int channels);

} // namespace media::ffmpeg::graph
