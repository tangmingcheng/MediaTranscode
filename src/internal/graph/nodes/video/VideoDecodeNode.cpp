#include "internal/graph/nodes/video/VideoDecodeNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

VideoDecodeNode::VideoDecodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
{
}

MediaNodeKind VideoDecodeNode::staticKind() noexcept
{
    return MediaNodeKind::VideoDecode;
}

::media::Status VideoDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!hasCodecContext()) {
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Status::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Status::success();
        }
        if (!tryBindCodecContext(*codecInput.value())) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("VideoDecodeNode expected codec context on codec input"));
        }
        return ::media::Status::success();
    }

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

    if (buffer->isEof() || buffer->isFlush()) {
        const int sendRet = avcodec_send_packet(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_packet(flush)" );
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
            ::media::ErrorInfo::invalidArgument("VideoDecodeNode expected packet buffer"));
    }

    const int sendRet = avcodec_send_packet(codecContext(), packet);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_packet(video)");
    }

    return receiveFrames(context);
}

::media::Status VideoDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoDecodeNode failed: av_frame_alloc returned null"));
        }

        const int ret = avcodec_receive_frame(codecContext(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_frame(video)");
        }

        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        auto pushStatus = pushToMatchingOutputs(context, buffer.value(), MediaStreamKind::Video);
        if (!pushStatus) {
            return pushStatus;
        }
    }
}

} // namespace media::ffmpeg::graph
