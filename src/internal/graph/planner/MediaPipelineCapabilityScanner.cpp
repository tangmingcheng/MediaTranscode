#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/capability/MediaAudioCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaInputCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaVideoCapabilityScanner.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputOptions.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
}

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

struct HardwareCapability {
    bool available = false;
    std::string reason;
};

using HardwareCapabilityCache = std::unordered_map<std::string, HardwareCapability>;

std::string ffmpegErrorString(int errorCode)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, text, sizeof(text));
    return text;
}

MediaRational mediaRationalFromAv(AVRational value) noexcept
{
    MediaRational rational;
    rational.num = value.num;
    rational.den = value.den;
    return rational;
}

MediaRational bestFrameRate(const AVStream* stream) noexcept
{
    if (!stream) {
        return {};
    }
    if (stream->avg_frame_rate.num != 0 && stream->avg_frame_rate.den != 0) {
        return mediaRationalFromAv(stream->avg_frame_rate);
    }
    if (stream->r_frame_rate.num != 0 && stream->r_frame_rate.den != 0) {
        return mediaRationalFromAv(stream->r_frame_rate);
    }
    return {};
}

FFmpegRealtimeInputOptions toFFmpegRealtimeInputOptions(const MediaPipelinePlannerOptions& options)
{
    FFmpegRealtimeInputOptions inputOptions;
    inputOptions.rtspTransport = options.rtspTransport;
    inputOptions.openTimeoutMs = options.openTimeoutMs;
    inputOptions.readTimeoutMs = options.readTimeoutMs;
    inputOptions.analyzeDurationUs = options.analyzeDurationUs;
    inputOptions.probeSizeBytes = options.probeSizeBytes;
    inputOptions.lowLatency = options.lowLatency;
    return inputOptions;
}

::media::Result<MediaInputVideoStreamInfo> detectVideoStreamInfoWithOptions(
    const std::string& inputPath,
    AVDictionary** inputOptions)
{
    auto opened = MediaInputCapabilityProbe::open(inputPath, inputOptions);
    if (!opened) return ::media::Result<MediaInputVideoStreamInfo>::failure(opened.error());
    ::media::ffmpeg::InputFormatContextPtr inputContext = std::move(opened).value();
    const int infoRet = avformat_find_stream_info(inputContext.get(), nullptr);
    if (infoRet < 0) {
        return ::media::Result<MediaInputVideoStreamInfo>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(infoRet), infoRet));
    }

    const int streamIndex = av_find_best_stream(inputContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return ::media::Result<MediaInputVideoStreamInfo>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(streamIndex), streamIndex));
    }

    const AVStream* stream = inputContext->streams[streamIndex];
    const AVCodecParameters* params = stream->codecpar;
    const char* codecName = avcodec_get_name(params->codec_id);
    if (!codecName || std::string(codecName) == "unknown") {
        return ::media::Result<MediaInputVideoStreamInfo>::failure(
            ::media::ErrorInfo::unsupported("input video codec is unknown"));
    }

    MediaInputVideoStreamInfo info;
    info.streamIndex = streamIndex;
    info.codecName = canonicalCodecName(codecName);
    info.width = params->width;
    info.height = params->height;
    info.bitrateBitsPerSecond = params->bit_rate;
    info.frameRate = bestFrameRate(stream);
    return ::media::Result<MediaInputVideoStreamInfo>::success(std::move(info));
}

::media::Result<MediaRealtimeInputStreamInfo> inspectRealtimeStreams(
    AVFormatContext& inputContext,
    bool includeAudio)
{
    const int videoIndex = av_find_best_stream(&inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIndex < 0) {
        return ::media::Result<MediaRealtimeInputStreamInfo>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(videoIndex), videoIndex));
    }

    AVStream* videoStream = inputContext.streams[videoIndex];
    const AVCodecParameters* videoParams = videoStream ? videoStream->codecpar : nullptr;
    const char* videoCodecName = videoParams ? avcodec_get_name(videoParams->codec_id) : nullptr;
    if (!videoCodecName || std::string(videoCodecName) == "unknown") {
        return ::media::Result<MediaRealtimeInputStreamInfo>::failure(
            ::media::ErrorInfo::unsupported("input video codec is unknown"));
    }

    MediaRealtimeInputStreamInfo info;
    info.video.streamIndex = videoIndex;
    info.video.codecName = canonicalCodecName(videoCodecName);
    info.video.width = videoParams->width;
    info.video.height = videoParams->height;
    info.video.bitrateBitsPerSecond = videoParams->bit_rate;
    info.video.frameRate = bestFrameRate(videoStream);
    if (!includeAudio) {
        return ::media::Result<MediaRealtimeInputStreamInfo>::success(std::move(info));
    }

    auto audio = MediaAudioCapabilityProbe::inspect(inputContext);
    if (!audio) return ::media::Result<MediaRealtimeInputStreamInfo>::failure(audio.error());
    info.hasAudio = audio.value().present;
    if (info.hasAudio) info.audio = std::move(audio).value().stream;
    return ::media::Result<MediaRealtimeInputStreamInfo>::success(std::move(info));
}

::media::Result<MediaRealtimeInputStreamInfo> detectRealtimeStreamInfoWithOptions(
    const std::string& inputPath,
    AVDictionary** inputOptions,
    bool includeAudio)
{
    auto opened = MediaInputCapabilityProbe::open(inputPath, inputOptions);
    if (!opened) return ::media::Result<MediaRealtimeInputStreamInfo>::failure(opened.error());
    ::media::ffmpeg::InputFormatContextPtr inputContext = std::move(opened).value();
    const int infoRet = avformat_find_stream_info(inputContext.get(), nullptr);
    if (infoRet < 0) {
        return ::media::Result<MediaRealtimeInputStreamInfo>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(infoRet), infoRet));
    }

    return inspectRealtimeStreams(*inputContext, includeAudio);
}


} // namespace

::media::Result<MediaInputVideoStreamInfo> MediaPipelineCapabilityScanner::detectInputVideoStreamInfo(
    const std::string& inputPath)
{
    return detectVideoStreamInfoWithOptions(inputPath, nullptr);
}

::media::Result<MediaInputVideoStreamInfo> MediaPipelineCapabilityScanner::detectRealtimeVideoStreamInfo(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options)
{
    AVDictionary* rawOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&rawOptions, toFFmpegRealtimeInputOptions(options));
    const auto cleanup = [&rawOptions]() {
        if (rawOptions) {
            av_dict_free(&rawOptions);
        }
    };

    auto result = detectVideoStreamInfoWithOptions(inputUrl, &rawOptions);
    cleanup();
    return result;
}

::media::Result<MediaRealtimeInputStreamInfo> MediaPipelineCapabilityScanner::detectRealtimeInputStreamInfo(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    bool includeAudio)
{
    AVDictionary* rawOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&rawOptions, toFFmpegRealtimeInputOptions(options));
    const auto cleanup = [&rawOptions]() {
        if (rawOptions) {
            av_dict_free(&rawOptions);
        }
    };

    auto result = detectRealtimeStreamInfoWithOptions(inputUrl, &rawOptions, includeAudio);
    cleanup();
    return result;
}

::media::Result<MediaPreparedRealtimeInputScan> MediaPipelineCapabilityScanner::prepareRealtimeInput(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    bool includeAudio)
{
    return prepareRealtimeInput(
        inputUrl, options, includeAudio,
        [](const std::string& url, AVDictionary** inputOptions)
            -> ::media::Result<::media::ffmpeg::InputFormatContextPtr> {
            return MediaInputCapabilityProbe::open(url, inputOptions);
        });
}

::media::Result<MediaPreparedRealtimeInputScan> MediaPipelineCapabilityScanner::prepareRealtimeInput(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    bool includeAudio,
    const MediaRealtimeInputOpener& opener)
{
    if (!opener) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::invalidArgument("prepareRealtimeInput requires opener"));
    }
    AVDictionary* rawOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&rawOptions, toFFmpegRealtimeInputOptions(options));
    auto opened = opener(inputUrl, &rawOptions);
    if (rawOptions) av_dict_free(&rawOptions);
    if (!opened) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(opened.error());
    }

    ::media::ffmpeg::InputFormatContextPtr input = std::move(opened).value();
    const int infoResult = avformat_find_stream_info(input.get(), nullptr);
    if (infoResult < 0) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(infoResult), infoResult));
    }
    auto streams = inspectRealtimeStreams(*input, includeAudio);
    if (!streams) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(streams.error());
    }
    auto prepared = MediaPreparedRealtimeInput::create(std::move(input));
    if (!prepared) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(prepared.error());
    }
    MediaPreparedRealtimeInputScan scan;
    scan.streams = std::move(streams).value();
    scan.prepared = std::move(prepared).value();
    return ::media::Result<MediaPreparedRealtimeInputScan>::success(std::move(scan));
}

std::vector<MediaPipelineChainPlan> MediaPipelineCapabilityScanner::enumerateVideoTranscodeCandidates(
    const std::string& inputCodecName,
    const std::string& outputCodecName,
    const MediaPipelinePlannerOptions& options)
{
    return MediaVideoCapabilityScanner::enumerateTranscodeCandidates(inputCodecName, outputCodecName, options);
}

} // namespace media::ffmpeg::graph
