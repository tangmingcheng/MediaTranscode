#include "common/MediaProbe.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
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

double rationalToDouble(AVRational value)
{
    if (value.num <= 0 || value.den <= 0) {
        return 0.0;
    }

    return av_q2d(value);
}

void updateVideoFrameCountFromPackets(AVFormatContext* formatContext,
                                      int videoStreamIndex,
                                      MediaProbeInfo& info)
{
    if (!formatContext || videoStreamIndex < 0 || info.videoFrameCount > 0) {
        return;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return;
    }

    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            ++info.videoFrameCount;
        }
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
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

    int firstVideoStreamIndex = -1;
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
                firstVideoStreamIndex = static_cast<int>(i);
                info.videoWidth = params->width;
                info.videoHeight = params->height;
                info.videoCodecName = avcodec_get_name(params->codec_id);
                info.videoAverageFps = rationalToDouble(stream->avg_frame_rate);
                if (info.videoAverageFps <= 0.0) {
                    info.videoAverageFps = rationalToDouble(stream->r_frame_rate);
                }
                if (stream->nb_frames > 0) {
                    info.videoFrameCount = stream->nb_frames;
                }
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

    updateVideoFrameCountFromPackets(formatContext, firstVideoStreamIndex, info);

    avformat_close_input(&formatContext);
    return true;
}

} // namespace media_transcode::test
