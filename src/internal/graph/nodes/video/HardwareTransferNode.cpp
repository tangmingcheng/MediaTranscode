#include "internal/graph/nodes/video/HardwareTransferNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

std::string pixelFormatName(int format)
{
    const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? std::string(name) : std::string("unknown");
}

void transferLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("hardware_transfer.") + message);
}

} // namespace

HardwareTransferNode::HardwareTransferNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "HardwareTransferNode")
{
}

MediaNodeKind HardwareTransferNode::staticKind() noexcept
{
    return MediaNodeKind::HardwareTransfer;
}

::media::Status HardwareTransferNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    const MediaBufferRef& buffer = *input.value();
    if (buffer->isEof() || buffer->isFlush()) {
        return emitOutput(context, "frame", buffer);
    }

    return transferOrForward(context, buffer);
}

::media::Status HardwareTransferNode::transferOrForward(MediaGraphExecutionContext& context,
                                                        const MediaBufferRef& buffer)
{
    const AVFrame* sourceFrame = FFmpegFrameView::frame(buffer);
    if (!sourceFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("HardwareTransferNode expected frame buffer"));
    }

    const std::string direction = optionValue(nodeOptions(context), "transfer.direction", "none");
    const bool hardwareInput = sourceFrame->hw_frames_ctx != nullptr;

    if (direction == "none") {
        auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                                   "hardware_transfer.none");
        if (decision.shouldLog) {
            std::ostringstream out;
            out << "none seq=" << decision.sequence
                << " fmt=" << pixelFormatName(sourceFrame->format)
                << " hardware_input=" << (hardwareInput ? "true" : "false")
                << " pts=" << sourceFrame->pts
                << " size=" << sourceFrame->width << "x" << sourceFrame->height;
            transferLog(MediaGraphDiagnosticLevel::Flow, out.str());
        }
        return emitOutput(context, "frame", buffer);
    }

    if (direction == "download") {
        if (!hardwareInput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("HardwareTransferNode planner requested download but input frame is not hardware-backed"));
        }
        return downloadHardwareFrame(context, buffer, sourceFrame);
    }

    if (direction == "upload" || direction == "map" || direction == "unmap") {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("HardwareTransferNode transfer direction is not implemented yet: " + direction));
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("HardwareTransferNode unknown transfer.direction: " + direction));
}

::media::Status HardwareTransferNode::downloadHardwareFrame(MediaGraphExecutionContext& context,
                                                            const MediaBufferRef& buffer,
                                                            const AVFrame* sourceFrame)
{
    auto softwareFrame = ::media::ffmpeg::makeFrame();
    if (!softwareFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("HardwareTransferNode failed: av_frame_alloc returned null"));
    }

    const int transferRet = av_hwframe_transfer_data(softwareFrame.get(), sourceFrame, 0);
    if (transferRet < 0) {
        return FFmpegGraphError::statusFromCode(transferRet, "av_hwframe_transfer_data(download)");
    }

    const int propsRet = av_frame_copy_props(softwareFrame.get(), sourceFrame);
    if (propsRet < 0) {
        return FFmpegGraphError::statusFromCode(propsRet, "av_frame_copy_props(downloaded frame)");
    }

    auto output = FFmpegBufferFactory::wrapFrame(std::move(softwareFrame), MediaStreamKind::Video);
    if (!output) {
        return ::media::Status::failure(output.error());
    }

    output.value()->setTimeDescriptor(buffer->timeDescriptor());
    if (const AVFrame* outFrame = FFmpegFrameView::frame(output.value())) {
        output.value()->setTimestamps(outFrame->pts, outFrame->pkt_dts, outFrame->duration);
    }

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "hardware_transfer.download");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "download seq=" << decision.sequence
            << " src_fmt=" << pixelFormatName(sourceFrame->format);
        if (const AVFrame* outFrame = FFmpegFrameView::frame(output.value())) {
            out << " dst_fmt=" << pixelFormatName(outFrame->format)
                << " pts=" << outFrame->pts
                << " size=" << outFrame->width << "x" << outFrame->height;
        }
        transferLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return emitOutput(context, "frame", output.value());
}

} // namespace media::ffmpeg::graph
