#include "internal/graph/nodes/audio/AudioEncodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

AudioEncodeNode::AudioEncodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioEncodeNode")
{
}

MediaNodeKind AudioEncodeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioEncode;
}

::media::Status AudioEncodeNode::start(MediaGraphExecutionContext& context) { resetRuntimeState(); return FFmpegCodecNodeRuntime::start(context); }
::media::Status AudioEncodeNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void AudioEncodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void AudioEncodeNode::resetRuntimeState() noexcept
{
    m_encoderConfigEmitted = false; m_terminals.reset(); m_eofEmitted = false; m_receivePending = false;
    m_flushPending = false; m_flushIsEof = false; m_flushSent = false; m_flushBuffer.reset();
    m_frameQueue.reset(); m_pendingFrame.reset();
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_flushPending) return continueFlush(context);
    if (m_receivePending) {
        auto receiveResult = receivePackets(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_pendingFrame || m_frameQueue.hasFullFrame()) {
        return encodeQueuedFrame(context, false);
    }
    if (m_terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (!hasCodecContext()) {
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        if (!tryBindCodecContext(*codecInput.value())) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument("AudioEncodeNode expected codec context on codec input"));
        }
        if (codecContext()->frame_size > 0) {
            auto queueStatus = m_frameQueue.configure(*codecContext());
            if (!queueStatus) {
                return ::media::Result<MediaNodeProcessResult>::failure(queueStatus.error());
            }
        }
        auto emitStatus = emitEncoderConfig(context, *codecInput.value());
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* frameInput = context.findInputChannel(nodeId(), "frame");
        if (frameInput && frameInput->closed()) {
            m_terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        auto emitStatus = emitEncoderConfig(context, buffer);
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_flushPending = true;
        m_flushIsEof = eof;
        m_flushSent = false;
        m_flushBuffer = buffer;
        return continueFlush(context);
    }

    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("AudioEncodeNode expected frame buffer"));
    }

    if (m_frameQueue.configured()) {
        auto queueStatus = m_frameQueue.push(*frame);
        if (!queueStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(queueStatus.error());
        }
        if (!m_frameQueue.hasFullFrame()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
        }
        return encodeQueuedFrame(context, false);
    }

    const int sendRet = avcodec_send_frame(codecContext(), frame);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(audio)"));
    }

    auto receiveStatus = receivePackets(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::encodeQueuedFrame(
    MediaGraphExecutionContext& context,
    bool allowPartial)
{
    if (!m_pendingFrame) {
        auto frame = m_frameQueue.hasFullFrame()
            ? m_frameQueue.popFullFrame()
            : (allowPartial ? m_frameQueue.popRemainingFrame()
                            : ::media::Result<::media::ffmpeg::FramePtr>::failure(
                                  ::media::ErrorInfo::wouldBlock("AudioEncodeNode has no complete queued frame")));
        if (!frame) {
            return ::media::Result<MediaNodeProcessResult>::failure(frame.error());
        }
        m_pendingFrame = std::move(frame).value();
    }

    const int sendRet = avcodec_send_frame(codecContext(), m_pendingFrame.get());
    if (sendRet == AVERROR(EAGAIN)) {
        auto receiveStatus = receivePackets(context);
        if (!receiveStatus) {
            return processProgress(::media::Status::failure(receiveStatus.error()));
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (sendRet < 0) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(audio queued)"));
    }
    m_pendingFrame.reset();

    auto receiveStatus = receivePackets(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Status AudioEncodeNode::emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (m_encoderConfigEmitted || !buffer) {
        return ::media::Status::success();
    }
    if (!context.findOutputChannel(nodeId(), "codec")) {
        m_encoderConfigEmitted = true;
        return ::media::Status::success();
    }
    auto status = emitOutput(context, "codec", buffer);
    if (!status) {
        return status;
    }
    m_encoderConfigEmitted = true;
    return ::media::Status::success();
}

::media::Result<bool> AudioEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("AudioEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = avcodec_receive_packet(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
        if (ret == AVERROR_EOF) return ::media::Result<bool>::success(true);

        if (ret < 0) {
            return ::media::Result<bool>::failure(FFmpegGraphError::fromCode(ret, "avcodec_receive_packet(audio)"));
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Audio);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }

        MediaTimeDescriptor timeDescriptor;
        timeDescriptor.timeBase = MediaRational{ codecContext()->time_base.num, codecContext()->time_base.den };
        buffer.value()->setTimeDescriptor(timeDescriptor);

        auto pushStatus = emitOutput(context, "packet", buffer.value());
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock && !m_flushPending) m_receivePending = true;
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (m_frameQueue.configured() &&
        (m_pendingFrame || m_frameQueue.queuedSamples() > 0)) {
        return encodeQueuedFrame(context, true);
    }
    if (!m_flushSent) {
        const int sendRet = avcodec_send_frame(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) m_flushSent = true;
        else if (sendRet != AVERROR(EAGAIN))
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(audio flush)"));
    }
    auto drain = receivePackets(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    const bool eof = m_flushIsEof;
    MediaBufferRef terminal = std::move(m_flushBuffer);
    m_flushPending = false;
    m_flushIsEof = false;
    m_flushSent = false;
    if (eof) { m_terminals.markEof("frame"); m_eofEmitted = true; }
    auto status = emitOutput(context, "packet", terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
