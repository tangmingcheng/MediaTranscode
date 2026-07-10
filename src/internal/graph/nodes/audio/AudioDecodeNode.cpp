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

::media::Result<MediaNodeProcessResult> AudioDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
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
        const int sendRet = avcodec_send_packet(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(audio flush)"));
        }
        auto drainStatus = receiveFrames(context);
        if (!drainStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(drainStatus.error());
        }
        auto broadcastStatus = broadcastControlToAllOutputs(context, buffer);
        if (!broadcastStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(broadcastStatus.error());
        }
        if (eof) {
            m_terminals.markEof("packet");
            m_eofEmitted = true;
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            eof ? MediaNodeProcessResult::finished() : MediaNodeProcessResult::progress());
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

::media::Status AudioDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("AudioDecodeNode failed: av_frame_alloc returned null"));
        }

        const int ret = avcodec_receive_frame(codecContext(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_frame(audio)");
        }

        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        if (codecContext()->pkt_timebase.num > 0 && codecContext()->pkt_timebase.den > 0) {
            MediaTimeDescriptor timeDescriptor;
            timeDescriptor.timeBase = MediaRational{ codecContext()->pkt_timebase.num, codecContext()->pkt_timebase.den };
            buffer.value()->setTimeDescriptor(timeDescriptor);
        }

        auto pushStatus = pushToMatchingOutputs(context, buffer.value(), MediaStreamKind::Audio);
        if (!pushStatus) {
            return pushStatus;
        }
    }
}

} // namespace media::ffmpeg::graph
