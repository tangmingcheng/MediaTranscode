#include "internal/graph/nodes/video/VideoFilterNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
}

#include <cstdio>
#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num > 0 && rational.den > 0;
}

AVRational toAVRational(MediaRational rational) noexcept
{
    return AVRational{ rational.num, rational.den };
}

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

bool startsWith(const std::string& value, const std::string& prefix) noexcept
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string rationalText(AVRational rational)
{
    if (!rationalKnown(rational)) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

AVRational sanitizeSampleAspectRatio(AVRational ratio) noexcept
{
    return rationalKnown(ratio) ? ratio : AVRational{ 1, 1 };
}

AVRational chooseInputFrameRate(const MediaBufferRef& buffer)
{
    if (buffer) {
        const MediaRational frameRate = buffer->timeDescriptor().frameRate;
        if (frameRate.isKnown()) {
            const AVRational avFrameRate = toAVRational(frameRate);
            const double fps = av_q2d(avFrameRate);
            if (fps > 1.0 && fps < 240.0) {
                return avFrameRate;
            }
        }
    }
    return AVRational{ 25, 1 };
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string();
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

::media::Status configureHardwareBufferSource(AVFilterGraph* graph,
                                              AVFilterContext** context,
                                              const AVFilter* bufferSrc,
                                              const AVFrame* firstFrame,
                                              AVRational timeBase,
                                              AVRational frameRate,
                                              AVRational pixelAspect)
{
    *context = avfilter_graph_alloc_filter(graph, bufferSrc, "in");
    if (!*context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: avfilter_graph_alloc_filter(buffer) returned null"));
    }

    AVBufferSrcParameters* parameters = av_buffersrc_parameters_alloc();
    if (!parameters) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_buffersrc_parameters_alloc returned null"));
    }

    parameters->format = firstFrame->format;
    parameters->width = firstFrame->width;
    parameters->height = firstFrame->height;
    parameters->time_base = timeBase;
    parameters->sample_aspect_ratio = pixelAspect;
    parameters->frame_rate = frameRate;
    parameters->hw_frames_ctx = av_buffer_ref(firstFrame->hw_frames_ctx);
    if (!parameters->hw_frames_ctx) {
        av_freep(&parameters);
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_buffer_ref(hw_frames_ctx)"));
    }

    const int setRet = av_buffersrc_parameters_set(*context, parameters);
    av_buffer_unref(&parameters->hw_frames_ctx);
    av_freep(&parameters);
    if (setRet < 0) {
        return FFmpegGraphError::statusFromCode(setRet, "av_buffersrc_parameters_set(video hardware buffer)");
    }

    const int initRet = avfilter_init_str(*context, nullptr);
    if (initRet < 0) {
        return FFmpegGraphError::statusFromCode(initRet, "avfilter_init_str(video hardware buffer)");
    }

    return ::media::Status::success();
}

::media::Status configureSoftwareBufferSource(AVFilterGraph* graph,
                                              AVFilterContext** context,
                                              const AVFilter* bufferSrc,
                                              const AVFrame* firstFrame,
                                              AVRational timeBase,
                                              AVRational frameRate,
                                              AVRational pixelAspect)
{
    char args[512] = {};
    std::snprintf(args,
                  sizeof(args),
                  "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                  firstFrame->width,
                  firstFrame->height,
                  firstFrame->format,
                  timeBase.num,
                  timeBase.den,
                  pixelAspect.num,
                  pixelAspect.den,
                  frameRate.num,
                  frameRate.den);

    const int ret = avfilter_graph_create_filter(context,
                                                 bufferSrc,
                                                 "in",
                                                 args,
                                                 nullptr,
                                                 graph);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_create_filter(buffer)");
    }

    return ::media::Status::success();
}

void filterLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_filter.") + message);
}

} // namespace

VideoFilterNode::VideoFilterNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFilterNode")
{
}

MediaNodeKind VideoFilterNode::staticKind() noexcept
{
    return MediaNodeKind::VideoFilter;
}

::media::Status VideoFilterNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_encoderContext) {
        MediaChannel* codecChannel = context.findInputChannel(nodeId(), "codec");
        if (!codecChannel) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("VideoFilterNode codec input channel not found"));
        }

        MediaBufferRef codecBuffer;
        if (!codecChannel->tryPop(codecBuffer)) {
            return ::media::Status::success();
        }
        return bindEncoderConfig(context, codecBuffer);
    }

    MediaChannel* frameChannel = context.findInputChannel(nodeId(), "frame");
    if (!frameChannel) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode frame input channel not found"));
    }

    MediaBufferRef frameBuffer;
    if (!frameChannel->tryPop(frameBuffer)) {
        return drainFrames(context);
    }

    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        auto flushStatus = flushGraph(context);
        if (!flushStatus) {
            return flushStatus;
        }
        return pushOutput(context, "frame", frameBuffer);
    }

    return sendFrame(context, frameBuffer);
}

::media::Status VideoFilterNode::bindEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected encoder codec context buffer"));
    }

    if (!rationalKnown(codecContext->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires encoder time_base"));
    }

    if (codecContext->pix_fmt == AV_PIX_FMT_NONE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires encoder pix_fmt"));
    }

    m_encoderConfig = buffer;
    m_encoderContext = codecContext;

    filterLog(MediaGraphDiagnosticLevel::State,
              std::string("bind_encoder codec_tb=") + rationalText(codecContext->time_base) +
                  " pix_fmt=" + pixelFormatName(codecContext->pix_fmt) +
                  " size=" + std::to_string(codecContext->width) + "x" + std::to_string(codecContext->height));

    if (MediaChannel* codecOut = context.findOutputChannel(nodeId(), "codec")) {
        auto status = codecOut->push(buffer);
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
}

::media::Status VideoFilterNode::initializeGraph(MediaGraphExecutionContext& context, const MediaBufferRef& firstFrameBuffer)
{
    const AVFrame* firstFrame = FFmpegFrameView::frame(firstFrameBuffer);
    if (!firstFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected first frame"));
    }

    if (!m_encoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode encoder context is not bound"));
    }

    const MediaRational inputTimeBase = firstFrameBuffer ? firstFrameBuffer->timeDescriptor().timeBase : MediaRational{};
    if (!inputTimeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires input frame time_base"));
    }

    const AVPixelFormat inputFormat = static_cast<AVPixelFormat>(firstFrame->format);
    if (inputFormat == AV_PIX_FMT_NONE || firstFrame->width <= 0 || firstFrame->height <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode first frame has invalid format or size"));
    }

    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    if (!bufferSrc || !bufferSink) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode failed to find buffer/buffersink filters"));
    }

    resetFilterGraph();
    m_filterGraph = ::media::ffmpeg::makeFilterGraph();
    if (!m_filterGraph) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: avfilter_graph_alloc returned null"));
    }

    m_inputTimeBase = toAVRational(inputTimeBase);
    const AVRational inputFrameRate = chooseInputFrameRate(firstFrameBuffer);
    const AVRational pixelAspect = sanitizeSampleAspectRatio(firstFrame->sample_aspect_ratio);
    const bool hardwareSource = firstFrame->hw_frames_ctx != nullptr;

    auto sourceStatus = hardwareSource
                            ? configureHardwareBufferSource(m_filterGraph.get(),
                                                            &m_bufferSrcContext,
                                                            bufferSrc,
                                                            firstFrame,
                                                            m_inputTimeBase,
                                                            inputFrameRate,
                                                            pixelAspect)
                            : configureSoftwareBufferSource(m_filterGraph.get(),
                                                            &m_bufferSrcContext,
                                                            bufferSrc,
                                                            firstFrame,
                                                            m_inputTimeBase,
                                                            inputFrameRate,
                                                            pixelAspect);
    if (!sourceStatus) {
        resetFilterGraph();
        return sourceStatus;
    }

    int ret = avfilter_graph_create_filter(&m_bufferSinkContext,
                                           bufferSink,
                                           "out",
                                           nullptr,
                                           nullptr,
                                           m_filterGraph.get());
    if (ret < 0) {
        resetFilterGraph();
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_create_filter(buffersink)");
    }

    const std::string filterName = plannedFilterName(nodeOptions(context));
    const std::string filterDescription = buildFilterDescription(nodeOptions(context));
    if (filterDescription.empty()) {
        resetFilterGraph();
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode failed to build filter description from planner"));
    }

    ::media::ffmpeg::FilterInOutPtr outputs = ::media::ffmpeg::makeFilterInOut();
    ::media::ffmpeg::FilterInOutPtr inputs = ::media::ffmpeg::makeFilterInOut();
    if (!outputs || !inputs) {
        resetFilterGraph();
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: avfilter_inout_alloc returned null"));
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_bufferSrcContext;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_bufferSinkContext;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (!outputs->name || !inputs->name) {
        resetFilterGraph();
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_strdup endpoint names"));
    }

    AVFilterInOut* inputsRaw = inputs.release();
    AVFilterInOut* outputsRaw = outputs.release();
    ret = avfilter_graph_parse_ptr(m_filterGraph.get(),
                                   filterDescription.c_str(),
                                   &inputsRaw,
                                   &outputsRaw,
                                   nullptr);
    inputs.reset(inputsRaw);
    outputs.reset(outputsRaw);
    if (ret < 0) {
        resetFilterGraph();
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_parse_ptr(video)");
    }

    ret = avfilter_graph_config(m_filterGraph.get(), nullptr);
    if (ret < 0) {
        resetFilterGraph();
        return FFmpegGraphError::statusFromCode(ret, "avfilter_graph_config(video)");
    }

    m_sinkTimeBase = av_buffersink_get_time_base(m_bufferSinkContext);
    if (!rationalKnown(m_sinkTimeBase)) {
        resetFilterGraph();
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode sink time_base is invalid"));
    }

    m_graphInitialized = true;
    m_flushed = false;

    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(m_inputTimeBase)
        << " sink_tb=" << rationalText(m_sinkTimeBase)
        << " encoder_tb=" << rationalText(m_encoderContext->time_base)
        << " input_fmt=" << pixelFormatName(inputFormat)
        << " encoder_fmt=" << pixelFormatName(m_encoderContext->pix_fmt)
        << " input_size=" << firstFrame->width << "x" << firstFrame->height
        << " encoder_size=" << m_encoderContext->width << "x" << m_encoderContext->height
        << " hardware_source=" << (hardwareSource ? "true" : "false")
        << " planner_filter=" << filterName
        << " desc=" << filterDescription;
    filterLog(MediaGraphDiagnosticLevel::State, out.str());

    return ::media::Status::success();
}

::media::Status VideoFilterNode::sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (!m_graphInitialized) {
        auto initStatus = initializeGraph(context, buffer);
        if (!initStatus) {
            return initStatus;
        }
    }

    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected frame buffer"));
    }

    const int ret = av_buffersrc_add_frame_flags(m_bufferSrcContext,
                                                 frame,
                                                 AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video)");
    }

    return drainFrames(context);
}

::media::Status VideoFilterNode::flushGraph(MediaGraphExecutionContext& context)
{
    if (!m_graphInitialized || m_flushed) {
        return ::media::Status::success();
    }

    const int ret = av_buffersrc_add_frame_flags(m_bufferSrcContext, nullptr, 0);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video flush)");
    }

    m_flushed = true;
    return drainFrames(context);
}

::media::Status VideoFilterNode::drainFrames(MediaGraphExecutionContext& context)
{
    if (!m_graphInitialized) {
        return ::media::Status::success();
    }

    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_frame_alloc returned null"));
        }

        const int ret = av_buffersink_get_frame(m_bufferSinkContext, frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "av_buffersink_get_frame(video)");
        }

        auto rescaleStatus = rescaleAndValidateFrame(frame.get());
        if (!rescaleStatus) {
            return rescaleStatus;
        }

        auto emitStatus = emitFrame(context, std::move(frame));
        if (!emitStatus) {
            return emitStatus;
        }
    }
}

::media::Status VideoFilterNode::emitFrame(MediaGraphExecutionContext& context, ::media::ffmpeg::FramePtr frame)
{
    auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{ m_encoderContext->time_base.num, m_encoderContext->time_base.den };
    buffer.value()->setTimeDescriptor(timeDescriptor);

    AVFrame* outputFrame = FFmpegFrameView::writableFrame(buffer.value());
    if (outputFrame) {
        buffer.value()->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    }

    return pushOutput(context, "frame", buffer.value());
}

::media::Status VideoFilterNode::rescaleAndValidateFrame(AVFrame* frame) noexcept
{
    if (!frame || !m_encoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame is invalid"));
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame has invalid pts"));
    }

    const int64_t ptsIn = frame->pts;
    frame->pts = av_rescale_q(frame->pts, m_sinkTimeBase, m_encoderContext->time_base);

    if (m_lastSubmittedPts != AV_NOPTS_VALUE && frame->pts <= m_lastSubmittedPts) {
        std::ostringstream out;
        out << "VideoFilterNode filtered timestamp is not strictly increasing current=" << frame->pts
            << " last=" << m_lastSubmittedPts;
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    m_lastSubmittedPts = frame->pts;

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_filter.frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "frame seq=" << decision.sequence
            << " sink_tb=" << rationalText(m_sinkTimeBase)
            << " encoder_tb=" << rationalText(m_encoderContext->time_base)
            << " pts_in=" << ptsIn
            << " pts_out=" << frame->pts
            << " fmt=" << pixelFormatName(static_cast<AVPixelFormat>(frame->format))
            << " duration=" << frame->duration;
        filterLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return ::media::Status::success();
}

void VideoFilterNode::resetFilterGraph() noexcept
{
    m_filterGraph.reset();
    m_bufferSrcContext = nullptr;
    m_bufferSinkContext = nullptr;
    m_inputTimeBase = AVRational{ 0, 1 };
    m_sinkTimeBase = AVRational{ 0, 1 };
    m_lastSubmittedPts = AV_NOPTS_VALUE;
    m_graphInitialized = false;
    m_flushed = false;
}

} // namespace media::ffmpeg::graph
