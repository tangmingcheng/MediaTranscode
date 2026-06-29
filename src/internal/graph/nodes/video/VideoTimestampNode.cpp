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
        return pushOutput(context, "frame", frameBuffer);
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
