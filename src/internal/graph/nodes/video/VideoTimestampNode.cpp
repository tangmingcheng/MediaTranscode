#include "internal/graph/nodes/video/VideoTimestampNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num != 0 && rational.den != 0;
}

std::string rationalText(AVRational rational)
{
    if (!rationalKnown(rational)) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

bool validTimestamp(int64_t value) noexcept
{
    return value != AV_NOPTS_VALUE && value != invalidMediaTimeValue;
}

int64_t rescaleTimestamp(int64_t value, AVRational source, AVRational target) noexcept
{
    return validTimestamp(value) ? av_rescale_q(value, source, target) : value;
}

AVRational decoderFrameTimeBase(const AVCodecContext* codecContext) noexcept
{
    if (!codecContext) {
        return AVRational{ 0, 1 };
    }

    if (rationalKnown(codecContext->pkt_timebase)) {
        return codecContext->pkt_timebase;
    }

    return codecContext->time_base;
}

void logTimestamp(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_timestamp.") + message);
}

} // namespace

VideoTimestampNode::VideoTimestampNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoTimestampNode")
{
}

MediaNodeKind VideoTimestampNode::staticKind() noexcept
{
    return MediaNodeKind::VideoTimestamp;
}

::media::Status VideoTimestampNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_hasSourceTimeBase) {
        MediaChannel* sourceCodecChannel = context.findInputChannel(nodeId(), "source_codec");
        if (!sourceCodecChannel) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("VideoTimestampNode source_codec input channel not found"));
        }

        MediaBufferRef sourceCodecBuffer;
        if (!sourceCodecChannel->tryPop(sourceCodecBuffer)) {
            return ::media::Status::success();
        }
        return bindSourceCodecConfig(context, sourceCodecBuffer);
    }

    if (!m_hasTargetTimeBase) {
        MediaChannel* targetCodecChannel = context.findInputChannel(nodeId(), "target_codec");
        if (!targetCodecChannel) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("VideoTimestampNode target_codec input channel not found"));
        }

        MediaBufferRef targetCodecBuffer;
        if (!targetCodecChannel->tryPop(targetCodecBuffer)) {
            return ::media::Status::success();
        }
        return bindTargetCodecConfig(context, targetCodecBuffer);
    }

    MediaChannel* frameChannel = context.findInputChannel(nodeId(), "frame");
    if (!frameChannel) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoTimestampNode frame input channel not found"));
    }

    MediaBufferRef frameBuffer;
    if (!frameChannel->tryPop(frameBuffer)) {
        return ::media::Status::success();
    }

    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        return pushToAllOutputs(context, frameBuffer);
    }

    return normalizeFrame(context, frameBuffer);
}

::media::Status VideoTimestampNode::bindSourceCodecConfig(MediaGraphExecutionContext&, const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected source codec context buffer"));
    }

    const AVRational sourceTimeBase = decoderFrameTimeBase(codecContext);
    if (!rationalKnown(sourceTimeBase)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode requires known source decoder time_base"));
    }

    m_sourceTimeBase = sourceTimeBase;
    m_hasSourceTimeBase = true;

    logTimestamp(MediaGraphDiagnosticLevel::State,
                 std::string("bind_source_time_base tb=") + rationalText(m_sourceTimeBase));
    return ::media::Status::success();
}

::media::Status VideoTimestampNode::bindTargetCodecConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected target codec context buffer"));
    }

    if (!rationalKnown(codecContext->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode requires known target encoder time_base"));
    }

    m_targetTimeBase = codecContext->time_base;
    m_hasTargetTimeBase = true;

    logTimestamp(MediaGraphDiagnosticLevel::State,
                 std::string("bind_target_time_base tb=") + rationalText(m_targetTimeBase));

    if (MediaChannel* codecOut = context.findOutputChannel(nodeId(), "target_codec")) {
        auto status = codecOut->push(buffer);
        if (!status) {
            return status;
        }
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("VideoTimestampNode target_codec output channel not found"));
}

::media::Status VideoTimestampNode::normalizeFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected frame buffer"));
    }

    const int64_t ptsIn = frame->pts;
    const int64_t dtsIn = frame->pkt_dts;
    const int64_t durationIn = frame->duration;

    if (m_sourceTimeBase.num != m_targetTimeBase.num || m_sourceTimeBase.den != m_targetTimeBase.den) {
        frame->pts = rescaleTimestamp(frame->pts, m_sourceTimeBase, m_targetTimeBase);
        frame->pkt_dts = rescaleTimestamp(frame->pkt_dts, m_sourceTimeBase, m_targetTimeBase);
        if (frame->duration > 0) {
            frame->duration = av_rescale_q(frame->duration, m_sourceTimeBase, m_targetTimeBase);
        }
    }

    MediaTimeDescriptor targetTime;
    targetTime.timeBase = MediaRational{ m_targetTimeBase.num, m_targetTimeBase.den };
    buffer->setTimeDescriptor(targetTime);
    buffer->setTimestamps(frame->pts, frame->pkt_dts, frame->duration);

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_timestamp.normalize_frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "normalize_frame seq=" << decision.sequence
            << " source_tb=" << rationalText(m_sourceTimeBase)
            << " target_tb=" << rationalText(m_targetTimeBase)
            << " pts_in=" << ptsIn
            << " dts_in=" << dtsIn
            << " duration_in=" << durationIn
            << " pts_out=" << frame->pts
            << " dts_out=" << frame->pkt_dts
            << " duration_out=" << frame->duration;
        logTimestamp(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    if (MediaChannel* frameOut = context.findOutputChannel(nodeId(), "frame")) {
        auto status = frameOut->push(buffer);
        if (!status) {
            return status;
        }
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("VideoTimestampNode frame output channel not found"));
}

} // namespace media::ffmpeg::graph
