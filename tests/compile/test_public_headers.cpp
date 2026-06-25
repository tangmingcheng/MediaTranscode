#include "media_transcode/MediaTypes.h"
#include "media_transcode/Result.h"
#include "media_transcode/LocalVideoTranscode.h"
#include "media_transcode/MediaTranscode.h"

int main()
{
    media::LocalVideoTranscodeConfig config;
    config.videoCodec = media::VideoCodec::H264;

    const media::ErrorInfo ok = media::ErrorInfo::success();
    return ok.ok() ? 0 : 1;
}
