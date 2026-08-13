#include "internal/graph/nodes/packet/PacketSourceConfigNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void packetSourceConfigLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("packet_source_config.") + message);
}

} // namespace

PacketSourceConfigNode::PacketSourceConfigNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketSourceConfigNode")
{
}

MediaNodeKind PacketSourceConfigNode::staticKind() noexcept
{
    return MediaNodeKind::PacketSourceConfig;
}

::media::Status PacketSourceConfigNode::stop(MediaGraphExecutionContext& context)
{
    releaseInputSnapshots();
    return FFmpegNodeRuntime::stop(context);
}

void PacketSourceConfigNode::abort(MediaGraphExecutionContext& context) noexcept
{
    releaseInputSnapshots();
    FFmpegNodeRuntime::abort(context);
}

::media::Result<MediaNodeProcessResult> PacketSourceConfigNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    if (!m_inputSnapshotOwner) {
        auto bindStatus = bindInputSnapshots(context);
        if (!bindStatus) {
            return processProgress(bindStatus);
        }
        if (!m_inputSnapshotOwner) {
            return processWaiting();
        }
    }

    if (m_sourceStreamIndex == invalidMediaStreamIndex) {
        auto streamStatus = bindSourceStream(context);
        if (!streamStatus) {
            return processProgress(streamStatus);
        }
    }

    return processFinished(emitSourceConfig(context));
}

void PacketSourceConfigNode::releaseInputSnapshots() noexcept
{
    m_inputSnapshotOwner.reset();
    m_sourceStream = nullptr;
    m_streamKind = MediaStreamKind::Unknown;
    m_sourceStreamIndex = invalidMediaStreamIndex;
    m_emitted = false;
}

::media::Status PacketSourceConfigNode::bindInputSnapshots(MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "format");
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    auto* formatBuffer = dynamic_cast<FFmpegInputSnapshotBuffer*>(buffer.get());
    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode expected complete input snapshots"));
    }

    m_inputSnapshotOwner = std::move(buffer);
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, "bind_input_snapshots");
    return ::media::Status::success();
}

::media::Status PacketSourceConfigNode::bindSourceStream(MediaGraphExecutionContext& context)
{
    auto streamKind = requiredStreamKindNodeOption(nodeOptions(context),
                                                   "PacketSourceConfigNode",
                                                   MediaTranscodeOptionKey::PacketStreamKind);
    if (!streamKind) {
        return ::media::Status::failure(streamKind.error());
    }
    auto streamIndex = requiredNonNegativeIntNodeOption(nodeOptions(context),
                                                        "PacketSourceConfigNode",
                                                        MediaTranscodeOptionKey::PacketSourceStreamIndex);
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_inputSnapshotOwner) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    const auto* formatBuffer = dynamic_cast<const FFmpegInputSnapshotBuffer*>(m_inputSnapshotOwner.get());
    m_sourceStream = formatBuffer ? formatBuffer->inputStreamSnapshot(index) : nullptr;
    if (!m_sourceStream || m_sourceStream->streamKind != streamKind.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode planner source stream kind does not match stream"));
    }

    m_streamKind = streamKind.value();
    m_sourceStreamIndex = index;

    std::ostringstream out;
    out << "bind_source_stream stream=" << mediaGraphDiagnosticStreamKindName(m_streamKind)
        << " index=" << m_sourceStreamIndex;
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());
    return ::media::Status::success();
}

::media::Status PacketSourceConfigNode::emitSourceConfig(MediaGraphExecutionContext& context)
{
    if (!m_sourceStream || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires source stream before emitting config"));
    }

    auto parameters = m_sourceStream->cloneCodecParameters();
    if (!parameters) {
        return ::media::Status::failure(parameters.error());
    }
    auto codecContext = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "PacketSourceConfigNode codec context allocation"));
    }
    const int converted = avcodec_parameters_to_context(
        codecContext.get(), parameters.value().get());
    if (converted < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure(
                "PacketSourceConfigNode codec parameter conversion failed",
                converted));
    }
    if (m_sourceStream->time.timeBase.num <= 0 ||
        m_sourceStream->time.timeBase.den <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "PacketSourceConfigNode source time base is unavailable"));
    }
    codecContext->time_base = AVRational{
        m_sourceStream->time.timeBase.num,
        m_sourceStream->time.timeBase.den};
    codecContext->pkt_timebase = codecContext->time_base;
    auto buffer = FFmpegBufferFactory::wrapCodecContext(
        std::move(codecContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    if (buffer.value()->streamKind() != m_streamKind) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode codec context stream kind mismatch"));
    }

    std::ostringstream out;
    const auto* contextBuffer =
        dynamic_cast<const FFmpegCodecContextBuffer*>(buffer.value().get());
    const AVCodecContext* codec = contextBuffer ? contextBuffer->context() : nullptr;
    if (!codec) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode codec context is unavailable"));
    }
    out << "emit_source_config stream=" << mediaGraphDiagnosticStreamKindName(m_streamKind)
        << " index=" << m_sourceStreamIndex
        << " time_base=" << m_sourceStream->time.timeBase.num << "/" << m_sourceStream->time.timeBase.den
        << " codec_id=" << codec->codec_id;
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());

    auto emitStatus = emitOutput(context, "codec", buffer.value());
    if (!emitStatus) {
        return emitStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
