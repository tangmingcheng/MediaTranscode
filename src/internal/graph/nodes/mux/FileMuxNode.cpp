#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/version.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

int registeredStreamIndexFor(MediaStreamKind streamKind,
                             int videoStreamIndex,
                             int audioStreamIndex) noexcept
{
    switch (streamKind) {
    case MediaStreamKind::Video:
        return videoStreamIndex;
    case MediaStreamKind::Audio:
        return audioStreamIndex;
    default:
        return invalidMediaStreamIndex;
    }
}

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num != 0 && rational.den != 0;
}

AVRational toAVRational(MediaRational rational) noexcept
{
    return AVRational{ rational.num, rational.den };
}

AVRational sourcePacketTimeBase(const MediaBufferRef& buffer) noexcept
{
    if (!buffer) {
        return AVRational{ 0, 1 };
    }

    const MediaRational packetTimeBase = buffer->timeDescriptor().timeBase;
    if (packetTimeBase.isKnown()) {
        return toAVRational(packetTimeBase);
    }

    const MediaRational formatTimeBase = buffer->formatDescriptor().time.timeBase;
    if (formatTimeBase.isKnown()) {
        return toAVRational(formatTimeBase);
    }

    return AVRational{ 0, 1 };
}

std::string rationalText(AVRational rational)
{
    if (!rationalKnown(rational)) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

int codecContextChannelCount(const AVCodecContext* context) noexcept
{
    if (!context) {
        return 0;
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return context->ch_layout.nb_channels;
#else
    return context->channels;
#endif
}

void muxLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("mux.") + message);
}

void muxSampleLog(MediaGraphDiagnosticLevel level,
                  const std::string& key,
                  const std::string& message)
{
    auto decision = mediaGraphDiagnosticSample(level, key);
    if (!decision.shouldLog) {
        return;
    }

    std::ostringstream out;
    out << message << " seq=" << decision.sequence;
    if (decision.sampled) {
        out << " sampled=1";
    }
    muxLog(level, out.str());
}

} // namespace

FileMuxNode::FileMuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
{
}

MediaNodeKind FileMuxNode::staticKind() noexcept
{
    return MediaNodeKind::FileMux;
}

::media::Status FileMuxNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = input.value();

    if (tryBindOutputContext(buffer)) {
        return registerPendingCodecContexts();
    }

    auto codecStatus = tryBindCodecContext(buffer);
    if (!codecStatus) {
        return codecStatus;
    }

    if (buffer->isEof() || buffer->isFlush()) {
        muxLog(MediaGraphDiagnosticLevel::State,
               std::string("control.defer_trailer ") + mediaGraphDiagnosticDescribeBuffer(buffer));
        return forwardIfOutputsExist(context, buffer);
    }

    if (FFmpegPacketView::isPacket(buffer)) {
        auto writeStatus = writePacket(buffer);
        if (!writeStatus) {
            return writeStatus;
        }
    }

    return forwardIfOutputsExist(context, buffer);
}

::media::Status FileMuxNode::flush(MediaGraphExecutionContext& context)
{
    muxLog(MediaGraphDiagnosticLevel::State, "flush.begin");
    auto status = writeTrailerIfNeeded();
    if (!status) {
        return status;
    }

    return FFmpegNodeRuntime::flush(context);
}

::media::Status FileMuxNode::stop(MediaGraphExecutionContext& context)
{
    muxLog(MediaGraphDiagnosticLevel::State, "stop.begin");
    auto status = writeTrailerIfNeeded();
    if (!status) {
        releaseRuntimeViews();
        return status;
    }

    auto stopStatus = FFmpegNodeRuntime::stop(context);
    releaseRuntimeViews();
    return stopStatus;
}

bool FileMuxNode::tryBindOutputContext(const MediaBufferRef& buffer) noexcept
{
    auto* contextBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!contextBuffer || !contextBuffer->context()) {
        return false;
    }

    if (contextBuffer->ownership() == FFmpegFormatContextOwnership::Output) {
        m_outputContextOwner = contextBuffer->takeOutputContext();
        m_outputContext = m_outputContextOwner.get();
    } else {
        m_outputContextOwner.reset();
        m_outputContext = contextBuffer->context();
    }

    if (!m_outputContext) {
        return false;
    }

    m_headerWritten = false;
    m_trailerWritten = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_audioStreamIndex = invalidMediaStreamIndex;

    muxLog(MediaGraphDiagnosticLevel::State,
           std::string("bind_output_context nb_streams=") +
               std::to_string(m_outputContext->nb_streams) + " ownership=" +
               (m_outputContextOwner ? "owned" : "borrowed") + " " +
               mediaGraphDiagnosticDescribeBuffer(buffer));
    return true;
}

::media::Status FileMuxNode::tryBindCodecContext(const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    if (!codecBuffer || !codecBuffer->context()) {
        return ::media::Status::success();
    }

    if (m_headerWritten) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode received codec context after header was written"));
    }

    if (!m_outputContext) {
        m_pendingCodecContexts.push_back(buffer);
        muxLog(MediaGraphDiagnosticLevel::State,
               std::string("queue_pending_config pending=") +
                   std::to_string(m_pendingCodecContexts.size()) + " " +
                   mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    return registerStreamFromCodecContext(buffer);
}

::media::Status FileMuxNode::registerPendingCodecContexts()
{
    if (!m_outputContext || m_pendingCodecContexts.empty()) {
        return ::media::Status::success();
    }

    auto pending = std::move(m_pendingCodecContexts);
    m_pendingCodecContexts.clear();
    muxLog(MediaGraphDiagnosticLevel::State,
           std::string("register_pending count=") + std::to_string(pending.size()));
    for (const auto& buffer : pending) {
        auto status = registerStreamFromCodecContext(buffer);
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
}

::media::Status FileMuxNode::registerStreamFromCodecContext(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        m_pendingCodecContexts.push_back(buffer);
        muxLog(MediaGraphDiagnosticLevel::State,
               std::string("queue_pending_config pending=") +
                   std::to_string(m_pendingCodecContexts.size()) + " " +
                   mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode expected FFmpegCodecContextBuffer"));
    }

    const MediaStreamKind streamKind = buffer->streamKind();
    if (registeredStreamIndexFor(streamKind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        muxLog(MediaGraphDiagnosticLevel::State,
               std::string("register_stream.skip_existing stream=") +
                   mediaGraphDiagnosticStreamKindName(streamKind) +
                   " " + mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("FileMuxNode failed: avformat_new_stream returned null"));
    }

    const int copyRet = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (copyRet < 0) {
        return FFmpegGraphError::statusFromCode(copyRet, "avcodec_parameters_from_context");
    }

    stream->time_base = codecContext->time_base;
    if (streamKind == MediaStreamKind::Video) {
        stream->avg_frame_rate = codecContext->framerate;
        stream->r_frame_rate = codecContext->framerate;
        m_videoStreamIndex = stream->index;
    } else if (streamKind == MediaStreamKind::Audio) {
        m_audioStreamIndex = stream->index;
    }

    std::ostringstream out;
    out << "register_stream stream=" << mediaGraphDiagnosticStreamKindName(streamKind)
        << " mux_stream_index=" << stream->index
        << " codec_id=" << codecContext->codec_id
        << " codec_tb=" << rationalText(codecContext->time_base)
        << " mux_tb=" << rationalText(stream->time_base)
        << " width=" << codecContext->width
        << " height=" << codecContext->height
        << " sample_rate=" << codecContext->sample_rate
        << " channels=" << codecContextChannelCount(codecContext)
        << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
    muxLog(MediaGraphDiagnosticLevel::State, out.str());

    return ::media::Status::success();
}

::media::Status FileMuxNode::writeHeaderIfNeeded()
{
    if (!m_outputContext || m_headerWritten) {
        return ::media::Status::success();
    }

    if (m_outputContext->nb_streams == 0) {
        muxLog(MediaGraphDiagnosticLevel::State, "write_header.wait_no_streams");
        return ::media::Status::success();
    }

    muxLog(MediaGraphDiagnosticLevel::State,
           std::string("write_header.begin nb_streams=") + std::to_string(m_outputContext->nb_streams));
    const int ret = avformat_write_header(m_outputContext, nullptr);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avformat_write_header");
    }

    m_headerWritten = true;
    muxLog(MediaGraphDiagnosticLevel::State, "write_header.done");
    return ::media::Status::success();
}

::media::Status FileMuxNode::writePacket(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        muxSampleLog(MediaGraphDiagnosticLevel::State,
                     std::string("mux.skip_no_output_context.") + mediaGraphDiagnosticStreamKindName(buffer->streamKind()),
                     std::string("write_packet.skip_no_output_context ") + mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    const AVPacket* sourcePacket = FFmpegPacketView::packet(buffer);
    if (!sourcePacket) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode expected packet buffer"));
    }

    const int targetStreamIndex = registeredStreamIndexFor(buffer->streamKind(),
                                                           m_videoStreamIndex,
                                                           m_audioStreamIndex);
    if (targetStreamIndex == invalidMediaStreamIndex) {
        muxSampleLog(MediaGraphDiagnosticLevel::State,
                     std::string("mux.drop_unregistered_stream.") + mediaGraphDiagnosticStreamKindName(buffer->streamKind()),
                     std::string("write_packet.drop_unregistered_stream stream=") +
                         mediaGraphDiagnosticStreamKindName(buffer->streamKind()) +
                         " source_stream_index=" + std::to_string(sourcePacket->stream_index) +
                         " " + mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    auto headerStatus = writeHeaderIfNeeded();
    if (!headerStatus) {
        return headerStatus;
    }

    if (!m_headerWritten) {
        muxSampleLog(MediaGraphDiagnosticLevel::State,
                     std::string("mux.wait_header.") + mediaGraphDiagnosticStreamKindName(buffer->streamKind()),
                     std::string("write_packet.wait_header stream=") +
                         mediaGraphDiagnosticStreamKindName(buffer->streamKind()) +
                         " " + mediaGraphDiagnosticDescribeBuffer(buffer));
        return ::media::Status::success();
    }

    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("FileMuxNode failed: av_packet_alloc returned null"));
    }

    const int refRet = av_packet_ref(packet.get(), sourcePacket);
    if (refRet < 0) {
        return FFmpegGraphError::statusFromCode(refRet, "av_packet_ref(mux packet)");
    }

    AVStream* muxStream = targetStreamIndex >= 0 && targetStreamIndex < static_cast<int>(m_outputContext->nb_streams)
                              ? m_outputContext->streams[targetStreamIndex]
                              : nullptr;
    const AVRational sourceTimeBase = sourcePacketTimeBase(buffer);
    const AVRational muxTimeBase = muxStream ? muxStream->time_base : AVRational{ 0, 1 };
    const int64_t ptsIn = packet->pts;
    const int64_t dtsIn = packet->dts;
    const int64_t durationIn = packet->duration;

    if (muxStream && rationalKnown(sourceTimeBase) && rationalKnown(muxTimeBase)) {
        av_packet_rescale_ts(packet.get(), sourceTimeBase, muxTimeBase);
    } else {
        muxSampleLog(MediaGraphDiagnosticLevel::State,
                     std::string("mux.timestamp_missing.") + mediaGraphDiagnosticStreamKindName(buffer->streamKind()),
                     std::string("timestamp.warning reason=missing_packet_time_base stream=") +
                         mediaGraphDiagnosticStreamKindName(buffer->streamKind()) +
                         " source_tb=" + rationalText(sourceTimeBase) +
                         " mux_tb=" + rationalText(muxTimeBase) +
                         " " + mediaGraphDiagnosticDescribeBuffer(buffer));
    }

    packet->stream_index = targetStreamIndex;

    std::ostringstream out;
    out << "write_packet stream=" << mediaGraphDiagnosticStreamKindName(buffer->streamKind())
        << " source_stream_index=" << sourcePacket->stream_index
        << " target_stream_index=" << targetStreamIndex
        << " source_tb=" << rationalText(sourceTimeBase)
        << " mux_tb=" << rationalText(muxTimeBase)
        << " pts_in=" << ptsIn
        << " dts_in=" << dtsIn
        << " duration_in=" << durationIn
        << " pts_out=" << packet->pts
        << " dts_out=" << packet->dts
        << " duration_out=" << packet->duration
        << " size=" << packet->size
        << " key=" << ((packet->flags & AV_PKT_FLAG_KEY) ? 1 : 0)
        << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
    muxSampleLog(MediaGraphDiagnosticLevel::Flow,
                 std::string("mux.write_packet.") + mediaGraphDiagnosticStreamKindName(buffer->streamKind()),
                 out.str());

    const int ret = av_interleaved_write_frame(m_outputContext, packet.get());
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_interleaved_write_frame");
    }

    return ::media::Status::success();
}

::media::Status FileMuxNode::writeTrailerIfNeeded()
{
    if (!m_outputContext || !m_headerWritten || m_trailerWritten) {
        return ::media::Status::success();
    }

    muxLog(MediaGraphDiagnosticLevel::State, "write_trailer.begin");
    const int ret = av_write_trailer(m_outputContext);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_write_trailer");
    }

    m_trailerWritten = true;
    muxLog(MediaGraphDiagnosticLevel::State, "write_trailer.done");
    return ::media::Status::success();
}

void FileMuxNode::releaseRuntimeViews() noexcept
{
    m_pendingCodecContexts.clear();
    m_outputContext = nullptr;
    m_outputContextOwner.reset();
    m_headerWritten = false;
    m_trailerWritten = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_audioStreamIndex = invalidMediaStreamIndex;
}

::media::Status FileMuxNode::forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (outputChannels(context).empty()) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, buffer);
}

} // namespace media::ffmpeg::graph
