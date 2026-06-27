#include "internal/graph/nodes/audio/AudioEncodeNode.h"

#include "internal/FFmpegRAII.h"
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

::media::Status AudioEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    if (tryBindCodecContext(input.value())) {
        return ::media::Status::success();
    }

    if (!hasCodecContext()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioEncodeNode requires codec context before frames"));
    }

    if (input.value()->isEof() || input.value()->isFlush()) {
        const int sendRet = avcodec_send_frame(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(audio flush)");
        }
        auto drainStatus = receivePackets(context);
        if (!drainStatus) {
            return drainStatus;
        }
        return pushToAllOutputs(context, input.value());
    }

    AVFrame* frame = FFmpegFrameView::writableFrame(input.value());
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioEncodeNode expected frame buffer"));
    }

    const int sendRet = avcodec_send_frame(codecContext(), frame);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(audio)");
    }

    return receivePackets(context);
}

::media::Status AudioEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("AudioEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = avcodec_receive_packet(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_packet(audio)");
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Audio);
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        auto pushStatus = pushToMatchingOutputs(context, buffer.value(), MediaStreamKind::Audio);
        if (!pushStatus) {
            return pushStatus;
        }
    }
}

} // namespace media::ffmpeg::graph
