#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class ScheduledRtpMuxStreamConfig;

class ScheduledRtpMuxFfmpegOptions final {
public:
    static ::media::Status apply(void* muxerPrivateData,
                                 const ScheduledRtpMuxStreamConfig& config);
    static ::media::Status verify(void* muxerPrivateData,
                                  const ScheduledRtpMuxStreamConfig& config);

private:
    ScheduledRtpMuxFfmpegOptions() = delete;
};

} // namespace media::ffmpeg::graph
