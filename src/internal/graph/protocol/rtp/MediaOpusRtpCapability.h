#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

::media::Status validateOpusRtpMappingFamilyZeroChannels(int channels);

} // namespace media::ffmpeg::graph
