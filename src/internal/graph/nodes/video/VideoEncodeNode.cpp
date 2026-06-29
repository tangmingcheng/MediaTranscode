#include "internal/graph/nodes/video/VideoEncodeNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

VideoEncodeNode::VideoEncodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoEncodeNode")
{
}

MediaNodeKind VideoEncodeNode::staticKind() noexcept
{
    return MediaNodeKind::VideoEncode;
}

::media::Status VideoEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    if (tryBindCodecContext(input.value())) {
        return emitEncoderConfig(context, input.value());
    }

    if (!hasCodecContext()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoEncodeNode requires codec context before frames"));
    }

    if (input.value()->isEof() || input.value()->isFlush()) {
        const int sendRet = avcodec_send_frame(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video flush)");
        }
        auto drainStatus = receivePackets(context);
        if (!drainStatus) {
            return drainStatus;
        }
        return pushOutput(context, "packet", input.value());
    }

    AVFrame* frame = FFmpegFrameView::writableFrame(input.value());
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode expected frame buffer"));
    }

    const int sendRet = avcodec_send_frame(codecContext(), frame);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video)");
    }

    return receivePackets(context);
}

::media::Status VideoEncodeNode::emitEncoderConfig(MediaGraphExecutionContext& context,
                                                   const MediaBufferRef& buffer)
{
    if (m_encoderConfigEmitted || !buffer) {
        return ::media::Status::success();
    }

    if (MediaChannel* codecChannel = context.findOutputChannel(nodeId(), "codec")) {
        auto status = codecChannel->push(buffer);
        if (!status) {
            return status;
        }
        m_encoderConfigEmitted = true;
        return ::media::Status::success();
    }

    MediaChannel* packetChannel = context.findOutputChannel(nodeId(), "packet");
    if (!packetChannel) {
        return ::media::Status::success();
    }

    auto status = packetChannel->push(buffer);
    if (!status) {
        return status;
    }

    m_encoderConfigEmitted = true;
    return ::media::Status::success();
}

::media::Status VideoEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = avcodec_receive_packet(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_packet(video)");
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Video);
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        MediaTimeDescriptor timeDescriptor;
        timeDescriptor.timeBase = MediaRational{ codecContext()->time_base.num, codecContext()->time_base.den };
        buffer.value()->setTimeDescriptor(timeDescriptor);

        auto pushStatus = pushOutput(context, "packet", buffer.value());
        if (!pushStatus) {
            return pushStatus;
        }
    }
}

} // namespace media::ffmpeg::graph
