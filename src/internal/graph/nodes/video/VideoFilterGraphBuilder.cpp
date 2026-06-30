#include "internal/graph/nodes/video/VideoFilterGraphBuilder.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/mem.h>
}

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num > 0 && rational.den > 0;
}

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

bool startsWith(const std::string& value, const std::string& prefix) noexcept
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string plannedFilterName(const MediaNodeOptions* options)
{
    std::string filter = optionValue(options, "filter.pipeline.filter");
    if (filter.empty()) {
        filter = optionValue(options, "filter.name");
    }
    if (filter.empty()) {
        filter = optionValue(options, "filter");
    }
    if (filter.empty()) {
        filter = "passthrough_software";
    }
    return filter;
}

std::string buildFilterDescription(const MediaNodeOptions* options)
{
    const std::string filter = plannedFilterName(options);
    if (filter.empty() || startsWith(filter, "passthrough")) {
        return "null";
    }
    return filter;
}

::media::Status validateRequest(const VideoFilterGraphBuildRequest& request)
{
    if (!request.firstFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder requires first frame"));
    }

    if (request.firstFrame->format == AV_PIX_FMT_NONE ||
        request.firstFrame->width <= 0 ||
        request.firstFrame->height <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder first frame has invalid format or size"));
    }

    if (!rationalKnown(request.inputTimeBase)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder requires input time base"));
    }

    if (!rationalKnown(request.inputFrameRate)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder requires input frame rate"));
    }

    if (!rationalKnown(request.sampleAspectRatio)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder requires sample aspect ratio"));
    }

    return ::media::Status::success();
}

::media::Status configureBufferSource(AVFilterGraph* graph,
                                      AVFilterContext** context,
                                      const AVFilter* bufferSource,
                                      const AVFrame* firstFrame,
                                      AVRational timeBase,
                                      AVRational frameRate,
                                      AVRational pixelAspect,
                                      bool hardwareSource)
{
    *context = avfilter_graph_alloc_filter(graph, bufferSource, "in");
    if (!*context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: avfilter_graph_alloc_filter(buffer) returned null"));
    }

    ::media::ffmpeg::BufferSrcParametersPtr parameters = ::media::ffmpeg::makeBufferSrcParameters();
    if (!parameters) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: av_buffersrc_parameters_alloc returned null"));
    }

    parameters->format = firstFrame->format;
    parameters->width = firstFrame->width;
    parameters->height = firstFrame->height;
    parameters->time_base = timeBase;
    parameters->sample_aspect_ratio = pixelAspect;
    parameters->frame_rate = frameRate;
    parameters->color_space = firstFrame->colorspace;
    parameters->color_range = firstFrame->color_range;

    if (hardwareSource) {
        parameters->hw_frames_ctx = av_buffer_ref(firstFrame->hw_frames_ctx);
        if (!parameters->hw_frames_ctx) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: av_buffer_ref(hw_frames_ctx)"));
        }
    }

    const int setRet = av_buffersrc_parameters_set(*context, parameters.get());
    if (setRet < 0) {
        return FFmpegGraphError::statusFromCode(setRet, "av_buffersrc_parameters_set(video buffer)");
    }

    const int initRet = avfilter_init_str(*context, nullptr);
    if (initRet < 0) {
        return FFmpegGraphError::statusFromCode(initRet, "avfilter_init_str(video buffer)");
    }

    return ::media::Status::success();
}

::media::Status configureBufferSink(AVFilterGraph* graph, AVFilterContext** context, const AVFilter* bufferSink)
{
    const int ret = avfilter_graph_create_filter(context,
                                                 bufferSink,
                                                 "out",
                                                 nullptr,
                                                 nullptr,
                                                 graph);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_create_filter(buffersink)");
    }

    return ::media::Status::success();
}

::media::Status parseFilterDescription(AVFilterGraph* graph,
                                       AVFilterContext* bufferSource,
                                       AVFilterContext* bufferSink,
                                       const std::string& description)
{
    ::media::ffmpeg::FilterInOutPtr outputs = ::media::ffmpeg::makeFilterInOut();
    ::media::ffmpeg::FilterInOutPtr inputs = ::media::ffmpeg::makeFilterInOut();
    if (!outputs || !inputs) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: avfilter_inout_alloc returned null"));
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = bufferSource;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = bufferSink;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (!outputs->name || !inputs->name) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: av_strdup endpoint names"));
    }

    AVFilterInOut* inputsRaw = inputs.release();
    AVFilterInOut* outputsRaw = outputs.release();
    const int ret = avfilter_graph_parse_ptr(graph,
                                             description.c_str(),
                                             &inputsRaw,
                                             &outputsRaw,
                                             nullptr);
    inputs.reset(inputsRaw);
    outputs.reset(outputsRaw);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_parse_ptr(video)");
    }

    return ::media::Status::success();
}

} // namespace

::media::Result<VideoFilterGraphBuildResult> VideoFilterGraphBuilder::build(
    const VideoFilterGraphBuildRequest& request)
{
    auto validation = validateRequest(request);
    if (!validation) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(validation.error());
    }

    const AVFilter* bufferSourceFilter = avfilter_get_by_name("buffer");
    const AVFilter* bufferSinkFilter = avfilter_get_by_name("buffersink");
    if (!bufferSourceFilter || !bufferSinkFilter) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterGraphBuilder failed to find buffer/buffersink filters"));
    }

    VideoFilterGraphBuildResult result;
    result.graph = ::media::ffmpeg::makeFilterGraph();
    if (!result.graph) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterGraphBuilder failed: avfilter_graph_alloc returned null"));
    }

    result.hardwareSource = request.firstFrame->hw_frames_ctx != nullptr;
    result.plannerFilter = plannedFilterName(request.options);
    result.filterDescription = buildFilterDescription(request.options);
    if (result.filterDescription.empty()) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder failed to build filter description from planner"));
    }

    auto sourceStatus = configureBufferSource(result.graph.get(),
                                             &result.bufferSource,
                                             bufferSourceFilter,
                                             request.firstFrame,
                                             request.inputTimeBase,
                                             request.inputFrameRate,
                                             request.sampleAspectRatio,
                                             result.hardwareSource);
    if (!sourceStatus) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(sourceStatus.error());
    }

    auto sinkStatus = configureBufferSink(result.graph.get(), &result.bufferSink, bufferSinkFilter);
    if (!sinkStatus) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(sinkStatus.error());
    }

    auto parseStatus = parseFilterDescription(result.graph.get(),
                                              result.bufferSource,
                                              result.bufferSink,
                                              result.filterDescription);
    if (!parseStatus) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(parseStatus.error());
    }

    const int configRet = avfilter_graph_config(result.graph.get(), nullptr);
    if (configRet < 0) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(
            FFmpegGraphError::statusFromCode(configRet, "avfilter_graph_config(video)").error());
    }

    result.sinkTimeBase = av_buffersink_get_time_base(result.bufferSink);
    if (!rationalKnown(result.sinkTimeBase)) {
        return ::media::Result<VideoFilterGraphBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterGraphBuilder sink time base is invalid"));
    }

    return ::media::Result<VideoFilterGraphBuildResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
