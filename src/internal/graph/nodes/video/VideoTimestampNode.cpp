#include "internal/graph/nodes/video/VideoTimestampNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
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

::media::Result<MediaNodeProcessResult> VideoTimestampNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (!m_hasSourceTimeBase) {
        auto sourceCodecInput = tryPopInputOptional(context, "source_codec");
        if (!sourceCodecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(sourceCodecInput.error());
        }
        if (!sourceCodecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        auto bindStatus = bindSourceCodecConfig(context, *sourceCodecInput.value());
        if (!bindStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(bindStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (!m_hasTargetTimeBase) {
        auto targetCodecInput = tryPopInputOptional(context, "target_codec");
        if (!targetCodecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(targetCodecInput.error());
        }
        if (!targetCodecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        auto bindStatus = bindTargetCodecConfig(context, *targetCodecInput.value());
        if (!bindStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(bindStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        MediaChannel* frameChannel = context.findInputChannel(nodeId(), "frame");
        if (frameChannel && frameChannel->closed()) {
            m_terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    MediaBufferRef frameBuffer = *frameInput.value();
    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        const bool eof = frameBuffer->isEof();
        if (eof && m_eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        auto emitStatus = emitOutput(context, "frame", frameBuffer);
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        if (eof) {
            m_terminals.markEof("frame");
            m_eofEmitted = true;
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            eof ? MediaNodeProcessResult::finished() : MediaNodeProcessResult::progress());
    }

    auto normalizeStatus = normalizeFrame(context, frameBuffer);
    if (!normalizeStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(normalizeStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
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

    auto synthesizeMissing = requiredBoolNodeOption(nodeOptions(context),
                                                    "VideoTimestampNode",
                                                    MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps);
    if (!synthesizeMissing) {
        return ::media::Status::failure(synthesizeMissing.error());
    }
    m_allowSyntheticMissingTimestamps = synthesizeMissing.value();

    logTimestamp(MediaGraphDiagnosticLevel::State,
                 std::string("bind_target_time_base tb=") + rationalText(m_targetTimeBase) +
                     " synthesize_missing=" + (m_allowSyntheticMissingTimestamps ? "1" : "0"));

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
    int64_t sourceTs = decodedTimestamp(frame);

    if (sourceTs == AV_NOPTS_VALUE) {
        if (!m_allowSyntheticMissingTimestamps) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("VideoTimestampNode input video frame has no timestamp"));
        }
        auto syntheticTs = nextSyntheticTimestamp(m_lastOutputTimestamp,
                                                  m_targetTimeBase,
                                                  m_sourceTimeBase);
        if (!syntheticTs) {
            return ::media::Status::failure(syntheticTs.error());
        }
        auto syntheticDuration = syntheticTimestampStep(m_targetTimeBase, m_sourceTimeBase);
        if (!syntheticDuration) {
            return ::media::Status::failure(syntheticDuration.error());
        }
        sourceTs = syntheticTs.value();
        frame->duration = syntheticDuration.value();
    }

    frame->pts = sourceTs;
    frame->pkt_dts = AV_NOPTS_VALUE;
    if (m_lastOutputTimestamp == AV_NOPTS_VALUE || sourceTs > m_lastOutputTimestamp) {
        m_lastOutputTimestamp = sourceTs;
    }

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
