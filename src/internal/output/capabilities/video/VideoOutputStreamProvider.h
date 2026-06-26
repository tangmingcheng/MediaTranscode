#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class VideoOutputStreamProvider {
public:
    virtual ~VideoOutputStreamProvider() = default;

    VideoOutputStreamProvider(const VideoOutputStreamProvider&) = delete;
    VideoOutputStreamProvider& operator=(const VideoOutputStreamProvider&) = delete;

    virtual bool requiresGlobalHeader() const = 0;
    virtual Result<AVStream*> createVideoStream(AVCodecContext* encoderCtx) = 0;

protected:
    VideoOutputStreamProvider() = default;
};

} // namespace media::ffmpeg
