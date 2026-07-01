#include "internal/graph/nodes/audio/AudioDecodeNode.h"

#include "internal/FFmpegRAII.h"
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

::media::Status AudioDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        return ::media::Status::success();
    }

    if (!hasCodecContext()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioDecodeNode requires codec context before packets"));
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const int sendRet = avcodec_send_packet(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_packet(audio flush)");
        }
        auto drainStatus = receiveFrames(context);
        if (!drainStatus) {
            return drainStatus;
        }
        return broadcastControlToAllOutputs(context, buffer);
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(buffer);
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioDecodeNode expected packet buffer"));
    }

    const int sendRet = avcodec_send_packet(codecContext(), packet);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_packet(audio)");
    }

    return receiveFrames(context);
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
