#include "internal/graph/planner/MediaPipelineAudioSourceProbe.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPipelineAudioSourceProbeResult> MediaPipelineAudioSourceProbe::probeFile(
    const std::string& inputPath)
{
    if (inputPath.empty()) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelineAudioSourceProbe requires input path"));
    }

    AVFormatContext* rawContext = nullptr;
    const int openRet = avformat_open_input(&rawContext, inputPath.c_str(), nullptr, nullptr);
    if (openRet < 0) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::failure(
            FFmpegGraphError::fromCode(openRet, "avformat_open_input(audio probe)"));
    }

    ::media::ffmpeg::InputFormatContextPtr formatContext(rawContext);
    const int streamInfoRet = avformat_find_stream_info(formatContext.get(), nullptr);
    if (streamInfoRet < 0) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::failure(
            FFmpegGraphError::fromCode(streamInfoRet, "avformat_find_stream_info(audio probe)"));
    }

    const int streamIndex = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex == AVERROR_STREAM_NOT_FOUND) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::success({});
    }
    if (streamIndex < 0) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::failure(
            FFmpegGraphError::fromCode(streamIndex, "av_find_best_stream(audio)"));
    }

    AVStream* stream = formatContext->streams[streamIndex];
    if (!stream || !stream->codecpar || stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        return ::media::Result<MediaPipelineAudioSourceProbeResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelineAudioSourceProbe selected stream is not audio"));
    }

    MediaPipelineAudioSourceProbeResult result;
    result.found = true;
    result.streamIndex = streamIndex;
    result.descriptor = FFmpegDescriptorMapper::fromStream(stream);
    if (!result.descriptor.codec.codecName.empty()) {
        result.codecName = result.descriptor.codec.codecName;
    } else {
        const char* codecName = avcodec_get_name(stream->codecpar->codec_id);
        result.codecName = codecName ? codecName : "";
    }

    return ::media::Result<MediaPipelineAudioSourceProbeResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
