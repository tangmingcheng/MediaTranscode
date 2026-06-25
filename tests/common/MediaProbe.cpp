#include "common/MediaProbe.h"

#include <sstream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media_transcode::test {
namespace {

void updateDuration(const AVFormatContext* formatContext, MediaProbeInfo& info)
{
    if (!formatContext || formatContext->duration == AV_NOPTS_VALUE || formatContext->duration <= 0) {
        return;
    }

    info.durationSeconds = static_cast<double>(formatContext->duration) / AV_TIME_BASE;
}

std::string ffmpegErrorText(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return buffer;
}

} // namespace

bool probeMediaFile(const std::string& path,
                    MediaProbeInfo& info,
                    std::string& errorMessage)
{
    info = {};
    errorMessage.clear();

    AVFormatContext* formatContext = nullptr;
    int ret = avformat_open_input(&formatContext, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        errorMessage = "avformat_open_input failed: " + ffmpegErrorText(ret);
        return false;
    }

    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        errorMessage = "avformat_find_stream_info failed: " + ffmpegErrorText(ret);
        avformat_close_input(&formatContext);
        return false;
    }

    updateDuration(formatContext, info);

    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        const AVStream* stream = formatContext->streams[i];
        if (!stream || !stream->codecpar) {
            continue;
        }

        const AVCodecParameters* params = stream->codecpar;
        if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
            ++info.videoStreamCount;
            if (!info.hasVideo) {
                info.hasVideo = true;
                info.videoWidth = params->width;
                info.videoHeight = params->height;
                info.videoCodecName = avcodec_get_name(params->codec_id);
            }
        }
        else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
            ++info.audioStreamCount;
            if (!info.hasAudio) {
                info.hasAudio = true;
                info.audioCodecName = avcodec_get_name(params->codec_id);
            }
        }
    }

    avformat_close_input(&formatContext);
    return true;
}

} // namespace media_transcode::test
