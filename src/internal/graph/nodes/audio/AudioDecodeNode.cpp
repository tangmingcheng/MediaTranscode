#include "internal/graph/nodes/audio/AudioDecodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

AudioDecodeNode::AudioDecodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioDecodeNode")
{
}

MediaNodeKind AudioDecodeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioDecode;
}

::media::Status AudioDecodeNode::start(MediaGraphExecutionContext& context) { resetRuntimeState(); return FFmpegCodecNodeRuntime::start(context); }
::media::Status AudioDecodeNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void AudioDecodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void AudioDecodeNode::resetRuntimeState() noexcept
{
    m_terminals.reset(); m_eofEmitted = false; m_receivePending = false; m_flushPending = false;
    m_flushIsEof = false; m_flushSent = false; m_flushBuffer.reset();
}

::media::Result<MediaNodeProcessResult> AudioDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_flushPending) return continueFlush(context);
    if (m_receivePending) {
        auto receiveResult = receiveFrames(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* packetInput = context.findInputChannel(nodeId(), "packet");
        if (packetInput && packetInput->closed()) {
            m_terminals.markClosed("packet");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (!hasCodecContext()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("AudioDecodeNode requires codec context before packets"));
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_flushPending = true; m_flushIsEof = eof; m_flushSent = false; m_flushBuffer = buffer;
        return continueFlush(context);
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(buffer);
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("AudioDecodeNode expected packet buffer"));
    }

    const int sendRet = avcodec_send_packet(codecContext(), packet);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(audio)"));
    }

    auto receiveStatus = receiveFrames(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<bool> AudioDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("AudioDecodeNode failed: av_frame_alloc returned null"));
        }

        const int ret = avcodec_receive_frame(codecContext(), frame.get());
        if (ret == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
        if (ret == AVERROR_EOF) return ::media::Result<bool>::success(true);

        if (ret < 0) {
            return ::media::Result<bool>::failure(FFmpegGraphError::fromCode(ret, "avcodec_receive_frame(audio)"));
        }

        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }

        if (codecContext()->pkt_timebase.num > 0 && codecContext()->pkt_timebase.den > 0) {
            MediaTimeDescriptor timeDescriptor;
            timeDescriptor.timeBase = MediaRational{ codecContext()->pkt_timebase.num, codecContext()->pkt_timebase.den };
            buffer.value()->setTimeDescriptor(timeDescriptor);
        }

        auto pushStatus = pushToMatchingOutputs(context, buffer.value(), MediaStreamKind::Audio);
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock && !m_flushPending) m_receivePending = true;
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> AudioDecodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (!m_flushSent) {
        const int sendRet = avcodec_send_packet(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) m_flushSent = true;
        else if (sendRet != AVERROR(EAGAIN)) return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(audio flush)"));
    }
    auto drain = receiveFrames(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    const bool eof = m_flushIsEof; MediaBufferRef terminal = std::move(m_flushBuffer);
    m_flushPending = false; m_flushIsEof = false; m_flushSent = false;
    if (eof) { m_terminals.markEof("packet"); m_eofEmitted = true; }
    auto status = broadcastControlToAllOutputs(context, terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
