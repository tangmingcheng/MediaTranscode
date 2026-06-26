#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class AudioOutputStreamProvider {
public:
    virtual ~AudioOutputStreamProvider() = default;

    AudioOutputStreamProvider(const AudioOutputStreamProvider&) = delete;
    AudioOutputStreamProvider& operator=(const AudioOutputStreamProvider&) = delete;

    virtual bool requiresGlobalHeader() const = 0;
    virtual Result<AVStream*> createAudioCopyStream(AVStream* inputAudioStream) = 0;
    virtual Result<AVStream*> createEncodedAudioStream(AVCodecContext* encoderCtx) = 0;

protected:
    AudioOutputStreamProvider() = default;
};

} // namespace media::ffmpeg
