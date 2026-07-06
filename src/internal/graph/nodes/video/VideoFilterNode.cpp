#include "internal/graph/nodes/video/VideoFilterNode.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>
#include <utility>

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

bool frameRateAcceptable(AVRational frameRate) noexcept
{
    if (!rationalKnown(frameRate)) {
        return false;
    }

    const double fps = av_q2d(frameRate);
    return fps > 1.0 && fps < 240.0;
}

AVRational chooseInputFrameRate(const MediaBufferRef& buffer, AVRational fallbackFrameRate) noexcept
{
    if (buffer) {
        const MediaRational frameRate = buffer->timeDescriptor().frameRate;
        if (frameRate.isKnown()) {
            const AVRational avFrameRate = toAVRational(frameRate);
            if (frameRateAcceptable(avFrameRate)) {
                return avFrameRate;
            }
        }
    }

    return frameRateAcceptable(fallbackFrameRate) ? fallbackFrameRate : AVRational{ 0, 1 };
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string();
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
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Status::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Status::success();
        }
        return bindEncoderConfig(context, *codecInput.value());
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Status::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        return drainFrames(context);
    }

    MediaBufferRef frameBuffer = *frameInput.value();
    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        auto flushStatus = flushGraph(context);
        if (!flushStatus) {
            return flushStatus;
        }
        return emitOutput(context, "frame", frameBuffer);
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

    if (context.findOutputChannel(nodeId(), "codec")) {
        return emitOutput(context, "codec", buffer);
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

    const AVRational inputFrameRate = chooseInputFrameRate(firstFrameBuffer, m_encoderContext->framerate);
    if (!rationalKnown(inputFrameRate)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode cannot resolve input frame rate; upstream must provide frame rate or encoder framerate"));
    }

    const AVRational pixelAspect = sanitizeSampleAspectRatio(firstFrame->sample_aspect_ratio);

    VideoFilterGraphBuildRequest request;
    request.options = nodeOptions(context);
    request.firstFrame = firstFrame;
    request.inputTimeBase = toAVRational(inputTimeBase);
    request.inputFrameRate = inputFrameRate;
    request.sampleAspectRatio = pixelAspect;

    auto graphResult = VideoFilterGraphBuilder::build(request);
    if (!graphResult) {
        resetFilterGraph();
        return ::media::Status::failure(graphResult.error());
    }

    VideoFilterGraphBuildResult built = std::move(graphResult).value();
    resetFilterGraph();
    m_filterGraph = std::move(built.graph);
    m_bufferSrcContext = built.bufferSource;
    m_bufferSinkContext = built.bufferSink;
    m_inputTimeBase = request.inputTimeBase;
    m_sinkTimeBase = built.sinkTimeBase;
    m_graphInitialized = true;
    m_flushed = false;

    const AVPixelFormat inputFormat = static_cast<AVPixelFormat>(firstFrame->format);
    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(m_inputTimeBase)
        << " input_fps=" << rationalText(inputFrameRate)
        << " sink_tb=" << rationalText(m_sinkTimeBase)
        << " encoder_tb=" << rationalText(m_encoderContext->time_base)
        << " input_fmt=" << pixelFormatName(inputFormat)
        << " encoder_fmt=" << pixelFormatName(m_encoderContext->pix_fmt)
        << " input_size=" << firstFrame->width << "x" << firstFrame->height
        << " encoder_size=" << m_encoderContext->width << "x" << m_encoderContext->height
        << " hardware_source=" << (built.hardwareSource ? "true" : "false")
        << " planner_filter=" << built.plannerFilter
        << " desc=" << built.filterDescription;
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

    return emitOutput(context, "frame", buffer.value());
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
    auto rescaledPts = rescaleStrictlyIncreasingTimestamp(frame->pts,
                                                          m_sinkTimeBase,
                                                          m_encoderContext->time_base,
                                                          m_lastSubmittedPts);
    if (!rescaledPts) {
        return ::media::Status::failure(rescaledPts.error());
    }

    frame->pts = rescaledPts.value();

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
