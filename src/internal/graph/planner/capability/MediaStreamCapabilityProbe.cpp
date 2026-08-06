#include "internal/graph/planner/capability/MediaStreamCapabilityProbe.h"

#include "internal/graph/planner/capability/MediaAudioCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaInputCapabilityProbe.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string ffmpegErrorString(int errorCode)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, text, sizeof(text));
    return text;
}

MediaRational frameRate(const AVStream& stream) noexcept
{
    const AVRational value = stream.avg_frame_rate.num != 0 && stream.avg_frame_rate.den != 0
        ? stream.avg_frame_rate
        : stream.r_frame_rate;
    return MediaRational{ value.num, value.den };
}

::media::Result<MediaInputVideoStreamInfo> inspectVideoContext(AVFormatContext& context)
{
    const int streamIndex = av_find_best_stream(&context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return ::media::Result<MediaInputVideoStreamInfo>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(streamIndex), streamIndex));
    }
    const AVStream* stream = context.streams[streamIndex];
    const AVCodecParameters* parameters = stream ? stream->codecpar : nullptr;
    const char* codecName = parameters ? avcodec_get_name(parameters->codec_id) : nullptr;
    if (!codecName || std::string(codecName) == "unknown") {
        return ::media::Result<MediaInputVideoStreamInfo>::failure(
            ::media::ErrorInfo::unsupported("input video codec is unknown"));
    }

    MediaInputVideoStreamInfo info;
    info.streamIndex = streamIndex;
    info.codecName = canonicalCodecName(codecName);
    info.width = parameters->width;
    info.height = parameters->height;
    info.bitrateBitsPerSecond = parameters->bit_rate;
    info.frameRate = frameRate(*stream);
    return ::media::Result<MediaInputVideoStreamInfo>::success(std::move(info));
}

::media::Result<::media::ffmpeg::InputFormatContextPtr> openAndInspect(
    const std::string& inputUrl, AVDictionary** inputOptions)
{
    auto opened = MediaInputCapabilityProbe::open(inputUrl, inputOptions);
    if (!opened) return opened;
    auto context = std::move(opened).value();
    const int result = avformat_find_stream_info(context.get(), nullptr);
    if (result < 0) {
        return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(result), result));
    }
    return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::success(std::move(context));
}

} // namespace

::media::Result<MediaInputVideoStreamInfo> MediaStreamCapabilityProbe::inspectVideo(
    const std::string& inputUrl, AVDictionary** inputOptions)
{
    auto context = openAndInspect(inputUrl, inputOptions);
    if (!context) return ::media::Result<MediaInputVideoStreamInfo>::failure(context.error());
    return inspectVideoContext(*context.value());
}

::media::Result<MediaRealtimeInputStreamInfo> MediaStreamCapabilityProbe::inspectRealtime(
    const std::string& inputUrl, AVDictionary** inputOptions,
    MediaTranscodeStreamSet streamSet)
{
    auto context = openAndInspect(inputUrl, inputOptions);
    if (!context) return ::media::Result<MediaRealtimeInputStreamInfo>::failure(context.error());
    auto video = inspectVideoContext(*context.value());
    if (!video) return ::media::Result<MediaRealtimeInputStreamInfo>::failure(video.error());

    MediaRealtimeInputStreamInfo info;
    info.video = std::move(video).value();
    if (streamSet == MediaTranscodeStreamSet::AudioVideo) {
        auto audio = MediaAudioCapabilityProbe::inspect(*context.value());
        if (!audio) return ::media::Result<MediaRealtimeInputStreamInfo>::failure(audio.error());
        info.hasAudio = audio.value().present;
        if (info.hasAudio) info.audio = std::move(audio).value().stream;
    }
    return ::media::Result<MediaRealtimeInputStreamInfo>::success(std::move(info));
}

::media::Result<MediaPreparedRealtimeInputScan> MediaStreamCapabilityProbe::prepareRealtime(
    const std::string& inputUrl, AVDictionary** inputOptions,
    MediaTranscodeStreamSet streamSet,
    const MediaRealtimeInputOpener& opener)
{
    if (!opener) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::invalidArgument("prepareRealtime requires opener"));
    }
    auto opened = opener(inputUrl, inputOptions);
    if (!opened) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(opened.error());
    auto input = std::move(opened).value();
    const int result = avformat_find_stream_info(input.get(), nullptr);
    if (result < 0) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(result), result));
    }

    auto video = inspectVideoContext(*input);
    if (!video) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(video.error());
    MediaRealtimeInputStreamInfo streams;
    streams.video = std::move(video).value();
    if (streamSet == MediaTranscodeStreamSet::AudioVideo) {
        auto audio = MediaAudioCapabilityProbe::inspect(*input);
        if (!audio) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(audio.error());
        streams.hasAudio = audio.value().present;
        if (streams.hasAudio) streams.audio = std::move(audio).value().stream;
    }
    auto prepared = MediaPreparedRealtimeInput::create(std::move(input));
    if (!prepared) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(prepared.error());

    MediaPreparedRealtimeInputScan scan;
    scan.streams = std::move(streams);
    scan.prepared = std::move(prepared).value();
    return ::media::Result<MediaPreparedRealtimeInputScan>::success(std::move(scan));
}

} // namespace media::ffmpeg::graph
