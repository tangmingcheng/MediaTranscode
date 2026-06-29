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

bool validTimestamp(int64_t value) noexcept
{
    return value != AV_NOPTS_VALUE && value != invalidMediaTimeValue;
}

int64_t rescaleTimestamp(int64_t value, AVRational source, AVRational target) noexcept
{
    return validTimestamp(value) ? av_rescale_q(value, source, target) : value;
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
    if (!m_hasTargetTimeBase) {
        MediaChannel* codecChannel = context.findInputChannel(nodeId(), "codec");
        if (!codecChannel) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("VideoTimestampNode codec input channel not found"));
        }

        MediaBufferRef codecBuffer;
        if (!codecChannel->tryPop(codecBuffer)) {
            return ::media::Status::success();
        }
        return bindCodecConfig(context, codecBuffer);
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

::media::Status VideoTimestampNode::bindCodecConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected codec context buffer"));
    }

    if (!rationalKnown(codecContext->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode requires known encoder time_base"));
    }

    m_targetTimeBase = codecContext->time_base;
    m_hasTargetTimeBase = true;

    logTimestamp(MediaGraphDiagnosticLevel::State,
                 std::string("bind_target_time_base tb=") + rationalText(m_targetTimeBase));

    if (MediaChannel* codecOut = context.findOutputChannel(nodeId(), "codec")) {
        auto status = codecOut->push(buffer);
        if (!status) {
            return status;
        }
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, buffer);
}

::media::Status VideoTimestampNode::normalizeFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode expected frame buffer"));
    }

    const MediaRational sourceRational = buffer->timeDescriptor().timeBase;
    if (!sourceRational.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoTimestampNode requires known input frame time_base"));
    }

    const AVRational sourceTimeBase = toAVRational(sourceRational);
    const int64_t ptsIn = frame->pts;
    const int64_t dtsIn = frame->pkt_dts;
    const int64_t durationIn = frame->duration;

    if (sourceTimeBase.num != m_targetTimeBase.num || sourceTimeBase.den != m_targetTimeBase.den) {
        frame->pts = rescaleTimestamp(frame->pts, sourceTimeBase, m_targetTimeBase);
        frame->pkt_dts = rescaleTimestamp(frame->pkt_dts, sourceTimeBase, m_targetTimeBase);
        if (frame->duration > 0) {
            frame->duration = av_rescale_q(frame->duration, sourceTimeBase, m_targetTimeBase);
        }
    }

    MediaTimeDescriptor targetTime = buffer->timeDescriptor();
    targetTime.timeBase = MediaRational{ m_targetTimeBase.num, m_targetTimeBase.den };
    targetTime.frameRate = MediaRational{};
    buffer->setTimeDescriptor(targetTime);
    buffer->setTimestamps(frame->pts, frame->pkt_dts, frame->duration);

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_timestamp.normalize_frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "normalize_frame seq=" << decision.sequence
            << " source_tb=" << rationalText(sourceTimeBase)
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

    return pushToAllOutputs(context, buffer);
}

} // namespace media::ffmpeg::graph
