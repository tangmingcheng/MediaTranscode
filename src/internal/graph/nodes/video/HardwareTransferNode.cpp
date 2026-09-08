#include "internal/graph/nodes/video/HardwareTransferNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

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

::media::Status HardwareTransferNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    auto direction = requiredNodeOption(
        nodeOptions(context), "HardwareTransferNode", "transfer.direction");
    if (!direction) return ::media::Status::failure(direction.error());

    if (direction.value() == "none") m_direction = Direction::None;
    else if (direction.value() == "download") m_direction = Direction::Download;
    else if (direction.value() == "upload") m_direction = Direction::Upload;
    else if (direction.value() == "map") m_direction = Direction::Map;
    else if (direction.value() == "unmap") m_direction = Direction::Unmap;
    else {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "HardwareTransferNode unknown transfer.direction: " + direction.value()));
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Status HardwareTransferNode::stop(MediaGraphExecutionContext& context)
{
    logSummary();
    auto status = FFmpegNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}

void HardwareTransferNode::abort(MediaGraphExecutionContext& context) noexcept
{
    FFmpegNodeRuntime::abort(context);
    resetRuntimeState();
}

void HardwareTransferNode::resetRuntimeState() noexcept
{
    m_terminals.reset();
    m_eofEmitted = false;
    m_firstInputDiagnosticEmitted = false;
    m_firstOutputDiagnosticEmitted = false;
    m_direction = Direction::None;
    m_pendingInput.reset();
    m_forwardedFrames = 0;
    m_downloads = 0;
    m_uploads = 0;
}

void HardwareTransferNode::logSummary() const
{
    std::ostringstream summary;
    summary << "summary forwarded=" << m_forwardedFrames
            << " download=" << m_downloads
            << " upload=" << m_uploads;
    transferLog(MediaGraphDiagnosticLevel::State, summary.str());
}

::media::Result<MediaNodeProcessResult> HardwareTransferNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    MediaBufferRef buffer = m_pendingInput;
    if (!buffer) {
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
        buffer = std::move(*input.value());
    }

    if (!m_firstInputDiagnosticEmitted) {
        transferLog(MediaGraphDiagnosticLevel::State,
                    "trace stage=first_input " +
                        mediaGraphDiagnosticDescribeBuffer(buffer));
        m_firstInputDiagnosticEmitted = true;
    }
    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        auto emitStatus = emitTracedOutput(context, buffer);
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        if (eof) {
            m_terminals.markEof("frame");
            m_eofEmitted = true;
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            eof ? MediaNodeProcessResult::finished() : MediaNodeProcessResult::progress());
    }

    m_pendingInput = buffer;
    auto transferStatus = transferOrForward(context, m_pendingInput);
    if (!transferStatus) {
        if (pendingOutputBufferCount() != 0) {
            m_pendingInput.reset();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(transferStatus.error());
    }
    m_pendingInput.reset();
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Status HardwareTransferNode::emitTracedOutput(
    MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto status = emitOutput(context, "frame", buffer);
    if (status && !m_firstOutputDiagnosticEmitted) {
        transferLog(MediaGraphDiagnosticLevel::State,
                    "trace stage=first_output " +
                        mediaGraphDiagnosticDescribeBuffer(buffer));
        m_firstOutputDiagnosticEmitted = true;
    }
    return status;
}

::media::Status HardwareTransferNode::transferOrForward(MediaGraphExecutionContext& context,
                                                        const MediaBufferRef& buffer)
{
    const AVFrame* sourceFrame = FFmpegFrameView::frame(buffer);
    if (!sourceFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("HardwareTransferNode expected frame buffer"));
    }

    const AVPixFmtDescriptor* pixelFormat =
        av_pix_fmt_desc_get(static_cast<AVPixelFormat>(sourceFrame->format));
    const bool hardwareInput = sourceFrame->hw_frames_ctx != nullptr ||
                               (pixelFormat && (pixelFormat->flags & AV_PIX_FMT_FLAG_HWACCEL));

    if (m_direction == Direction::None) {
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
        auto status = emitTracedOutput(context, buffer);
        if (status) ++m_forwardedFrames;
        return status;
    }

    if (m_direction == Direction::Download) {
        if (!hardwareInput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("HardwareTransferNode planner requested download but input frame is not hardware-backed"));
        }
        return downloadHardwareFrame(context, buffer, sourceFrame);
    }

    if (m_direction == Direction::Upload ||
        m_direction == Direction::Map ||
        m_direction == Direction::Unmap) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "HardwareTransferNode planner-requested transfer direction is not implemented"));
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("HardwareTransferNode has no bound transfer direction"));
}

::media::Status HardwareTransferNode::downloadHardwareFrame(MediaGraphExecutionContext& context,
                                                            const MediaBufferRef& buffer,
                                                            const AVFrame* sourceFrame)
{
    auto reservation = context.reservePayload(
        nodeId(), MediaStreamKind::Video, MediaPayloadKind::Frame);
    if (!reservation) {
        return ::media::Status::failure(reservation.error());
    }
    auto softwareFrame = ::media::ffmpeg::makeFrame();
    if (!softwareFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("HardwareTransferNode failed: av_frame_alloc returned null"));
    }

    const int transferRet = av_hwframe_transfer_data(softwareFrame.get(), sourceFrame, 0);
    if (transferRet < 0) {
        return FFmpegGraphError::statusFromCode(transferRet, "av_hwframe_transfer_data(download)");
    }
    ++m_downloads;

    const int propsRet = av_frame_copy_props(softwareFrame.get(), sourceFrame);
    if (propsRet < 0) {
        return FFmpegGraphError::statusFromCode(propsRet, "av_frame_copy_props(downloaded frame)");
    }

    auto output = FFmpegBufferFactory::wrapFrame(std::move(softwareFrame), MediaStreamKind::Video);
    if (!output) {
        return ::media::Status::failure(output.error());
    }
    const AVFrame* transferredFrame = FFmpegFrameView::frame(output.value());
    auto footprint = transferredFrame
        ? MediaFramePayloadFootprint::logicalBytes(
              *transferredFrame, MediaStreamKind::Video)
        : ::media::Result<std::uint64_t>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "HardwareTransferNode wrapped frame is unavailable"));
    if (!footprint) return ::media::Status::failure(footprint.error());
    if (auto status = reservation.value().shrinkToActual(
            footprint.value()); !status) {
        return status;
    }
    if (auto status = reservation.value().attachTo(*output.value());
        !status) {
        return status;
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

    MediaBufferRef transferred = output.value();
    if (auto lineage = FFmpegFrameView::canonicalLineage(buffer)) {
        auto canonical = MediaCanonicalVideoFrameBuffer::create(
            transferred, std::move(lineage));
        if (!canonical) return ::media::Status::failure(canonical.error());
        transferred = std::move(canonical).value();
    }
    return emitTracedOutput(context, transferred);
}

} // namespace media::ffmpeg::graph
