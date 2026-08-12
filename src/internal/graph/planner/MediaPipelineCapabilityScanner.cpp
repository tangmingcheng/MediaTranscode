#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/capability/MediaInputCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaStreamCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaVideoCapabilityScanner.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputOptions.h"

extern "C" {
#include <libavutil/dict.h>
}

#include <string>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

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

} // namespace

::media::Result<MediaInputVideoStreamInfo> MediaPipelineCapabilityScanner::detectInputVideoStreamInfo(
    const std::string& inputPath)
{
    return MediaStreamCapabilityProbe::inspectVideo(inputPath, nullptr);
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

    auto result = MediaStreamCapabilityProbe::inspectVideo(inputUrl, &rawOptions);
    cleanup();
    return result;
}

::media::Result<MediaRealtimeInputStreamInfo> MediaPipelineCapabilityScanner::detectRealtimeInputStreamInfo(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    MediaTranscodeStreamSet streamSet)
{
    AVDictionary* rawOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&rawOptions, toFFmpegRealtimeInputOptions(options));
    const auto cleanup = [&rawOptions]() {
        if (rawOptions) {
            av_dict_free(&rawOptions);
        }
    };

    auto result = MediaStreamCapabilityProbe::inspectRealtime(inputUrl, &rawOptions, streamSet);
    cleanup();
    return result;
}

::media::Result<MediaPreparedRealtimeInputScan> MediaPipelineCapabilityScanner::prepareRealtimeInput(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    MediaTranscodeStreamSet streamSet,
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaAvSyncStartupPolicy& startup)
{
    return prepareRealtimeInput(
        inputUrl, options, streamSet,
        [](const std::string& url, AVDictionary** inputOptions)
            -> ::media::Result<::media::ffmpeg::InputFormatContextPtr> {
            return MediaInputCapabilityProbe::open(url, inputOptions);
        }, request, startup);
}

::media::Result<MediaPreparedRealtimeInputScan> MediaPipelineCapabilityScanner::prepareRealtimeInput(
    const std::string& inputUrl,
    const MediaPipelinePlannerOptions& options,
    MediaTranscodeStreamSet streamSet,
    const MediaRealtimeInputOpener& opener,
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaAvSyncStartupPolicy& startup)
{
    if (!opener) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::invalidArgument("prepareRealtimeInput requires opener"));
    }
    AVDictionary* rawOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&rawOptions, toFFmpegRealtimeInputOptions(options));
    auto result = MediaStreamCapabilityProbe::prepareRealtime(
        inputUrl, &rawOptions, streamSet, opener, request, startup);
    if (rawOptions) av_dict_free(&rawOptions);
    return result;
}

std::vector<MediaPipelineChainPlan> MediaPipelineCapabilityScanner::enumerateVideoTranscodeCandidates(
    const std::string& inputCodecName,
    const std::string& outputCodecName,
    const MediaPipelinePlannerOptions& options)
{
    return MediaVideoCapabilityScanner::enumerateTranscodeCandidates(inputCodecName, outputCodecName, options);
}

} // namespace media::ffmpeg::graph
