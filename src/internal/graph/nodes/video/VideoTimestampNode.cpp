#include "internal/graph/nodes/video/VideoTimestampNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
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

int64_t decodedTimestamp(const AVFrame* frame) noexcept
{
    if (!frame) {
        return AV_NOPTS_VALUE;
    }
    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        return frame->best_effort_timestamp;
    }
    if (frame->pts != AV_NOPTS_VALUE) {
        return frame->pts;
    }
    if (frame->pkt_dts != AV_NOPTS_VALUE) {
        return frame->pkt_dts;
    }
    return AV_NOPTS_VALUE;
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
        auto sourceCodecInput = tryPopInputOptional(context, "source_codec");
        if (!sourceCodecInput) {
            return ::media::Status::failure(sourceCodecInput.error());
        }
        if (!sourceCodecInput.value()) {
            return ::media::Status::success();
        }
        return bindSourceCodecConfig(context, *sourceCodecInput.value());
    }

    if (!m_hasTargetTimeBase) {
        auto targetCodecInput = tryPopInputOptional(context, "target_codec");
        if (!targetCodecInput) {
            return ::media::Status::failure(targetCodecInput.error());
        }
        if (!targetCodecInput.value()) {
            return ::media::Status::success();
        }
        return bindTargetCodecConfig(context, *targetCodecInput.value());
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Status::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef frameBuffer = *frameInput.value();
    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        return emitOutput(context, "frame", frameBuffer);
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

    if (!rationalKnown(codecContext->pkt_timebase)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode requires source stream time_base"));
    }

    m_sourceTimeBase = codecContext->pkt_timebase;
    m_hasSourceTimeBase = true;

    std::ostringstream out;
    out << "bind_source_time_base tb=" << rationalText(m_sourceTimeBase)
        << " decoder_tb=" << rationalText(codecContext->time_base)
        << " pkt_tb=" << rationalText(codecContext->pkt_timebase)
        << " mode=input_stream_time_base";
    logTimestamp(MediaGraphDiagnosticLevel::State, out.str());
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

    return emitOutput(context, "target_codec", buffer);
}

::media::Status VideoTimestampNode::normalizeFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected frame buffer"));
    }

    const int64_t ptsIn = frame->pts;
    const int64_t bestIn = frame->best_effort_timestamp;
    const int64_t dtsIn = frame->pkt_dts;
    const int64_t durationIn = frame->duration;
    const int64_t sourceTs = decodedTimestamp(frame);

    if (sourceTs == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode input video frame has no timestamp"));
    }

    frame->pts = sourceTs;
    frame->pkt_dts = AV_NOPTS_VALUE;

    MediaTimeDescriptor sourceTime;
    sourceTime.timeBase = MediaRational{ m_sourceTimeBase.num, m_sourceTimeBase.den };
    buffer->setTimeDescriptor(sourceTime);
    buffer->setTimestamps(frame->pts, frame->pkt_dts, frame->duration);

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_timestamp.normalize_frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "normalize_frame seq=" << decision.sequence
            << " mode=input_stream_time_base"
            << " source_tb=" << rationalText(m_sourceTimeBase)
            << " encoder_tb=" << rationalText(m_targetTimeBase)
            << " source_ts=" << sourceTs
            << " best_in=" << bestIn
            << " pts_in=" << ptsIn
            << " dts_in=" << dtsIn
            << " duration_in=" << durationIn
            << " pts_out=" << frame->pts
            << " dts_out=" << frame->pkt_dts
            << " duration_out=" << frame->duration;
        logTimestamp(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return emitOutput(context, "frame", buffer);
}

} // namespace media::ffmpeg::graph
