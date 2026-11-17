#include "internal/graph/nodes/video/HardwareTransferNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaVideoDisplayLineage.h"

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
    m_bindingFrozen = true;
    freezeSourceReferenceResource();
    resetRuntimeState();
    if (m_purge) {
        if (auto status = m_purge->start(context.sharedNodeWakeup(nodeId())); !status) return status;
    }
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
    if (m_purge) m_purge->stop();
    logSummary();
    auto status = FFmpegNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}

void HardwareTransferNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_purge) m_purge->stop();
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
    m_generationEnded = false;
    m_generation = 0;
    m_releaseIdentity = 0;
    if (m_lineage) m_lineage->resetGenerationLifecycle();
}

::media::Status HardwareTransferNode::bindSourceGroup(std::shared_ptr<MediaAvSyncGroupRuntime> group,
    std::shared_ptr<MediaAvStartupVideoPreparationState> preparation)
{
    if (!group || !preparation || preparation->snapshot().groupKey != group->key() ||
        m_bindingFrozen || m_group) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Hardware transfer requires one immutable source group binding"));
    m_group = std::move(group);
    m_preparation = std::move(preparation);
    m_purge = std::make_shared<MediaOwnerThreadGenerationPurge>();
    m_lineage = std::make_unique<MediaVideoReferenceLineageState>();
    return ::media::Status::success();
}

std::optional<std::uint64_t> HardwareTransferNode::generation(const MediaBufferRef& buffer) const noexcept
{
    if (const auto lineage = FFmpegFrameView::canonicalLineage(buffer)) return lineage->generation;
    if (const auto display = FFmpegFrameView::displayLineage(buffer); display && display->accessUnit)
        return display->accessUnit->generation;
    return std::nullopt;
}

bool HardwareTransferNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    return !m_lineage || m_lineage->pendingOutputIsCurrent(buffer, generation(buffer));
}

::media::Result<MediaOutputCommitReservation> HardwareTransferNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    if (!m_group) return FFmpegNodeRuntime::reserveOutputCommit(buffer);
    auto authority = m_group->reserveGenerationArbitration();
    if (!authority) return Result::failure(authority.error());
    if (!pendingOutputIsCurrent(buffer)) return Result::failure(
        ::media::ErrorInfo::cancelled("Hardware transfer retires stale publication"));
    const auto end = mediaEndGeneration(buffer);
    if (!end) return Result::failure(end.error());
    const auto mediaGeneration = generation(buffer);
    const auto value = end.value() ? *end.value() : mediaGeneration.value_or(m_generation);
    const auto& transition = authority.value().reacquisition();
    const auto& epoch = authority.value().epoch();
    const auto preparation = m_preparation->snapshot();
    if (preparation.phase == MediaAvStartupVideoPreparationPhase::Cancelled)
        return Result::failure(::media::ErrorInfo::cancelled("Hardware transfer preparation was cancelled"));
    const bool initial = transition.phase == MediaAvReacquisitionPhase::Inactive &&
        !transition.transition && !epoch.poisoned && !epoch.activation &&
        epoch.readiness == MediaAvGenerationReadiness::Acquiring &&
        !epoch.outputPermitted && !epoch.completedTransitionSequence;
    if (!initial) {
        auto disposition = classifyMediaAvGenerationEvidence(transition, epoch, value);
        if (!disposition) return Result::failure(disposition.error());
        if (disposition.value() == MediaAvGenerationEvidenceDisposition::Retired)
            return Result::failure(::media::ErrorInfo::cancelled("Hardware transfer retires obsolete generation"));
        if (disposition.value() == MediaAvGenerationEvidenceDisposition::Future ||
            transition.phase == MediaAvReacquisitionPhase::Purging)
            return Result::failure(::media::ErrorInfo::wouldBlock("Hardware transfer awaits generation authority"));
    }
    if (!value || preparation.groupKey != m_group->key() || preparation.generation != value ||
        !preparation.releaseIdentity || preparation.releaseIdentity != m_releaseIdentity)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Hardware transfer publication differs from its source preparation"));
    if (initial || !epoch.outputPermitted) {
        if (preparation.phase != MediaAvStartupVideoPreparationPhase::Feeding &&
            preparation.phase != MediaAvStartupVideoPreparationPhase::OutputReady)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Hardware transfer has no active preparation feed"));
    } else if (preparation.phase != MediaAvStartupVideoPreparationPhase::ReleaseCommitted) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Hardware transfer active generation has no committed preparation release"));
    }
    if (end.value()) {
        const auto closing = m_group->sourceDrainSnapshot();
        if (!m_generationEnded || !epoch.outputPermitted || !closing ||
            closing->request.playbackGeneration != value || !closing->video.acceptedFrontier)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Hardware transfer generation end requires its published sealed source"));
    }
    return Result::success(MediaOutputCommitReservation::hold(
        std::move(authority).value().retainPublicationAuthority()));
}

::media::Status HardwareTransferNode::servicePurge()
{
    if (!m_purge) return ::media::Status::success();
    const auto request = m_purge->pending();
    if (!request) return ::media::Status::success();
    auto status = m_lineage->purge(*request);
    if (status) {
        cancelPendingOutputTransfer();
        m_pendingInput.reset();
        m_generationEnded = false;
        m_generation = request->nextGeneration;
        m_releaseIdentity = 0;
    }
    auto completed = m_purge->complete(*request, status);
    return completed ? status : completed;
}

::media::Result<MediaNodeProcessResult> HardwareTransferNode::process(MediaGraphExecutionContext& context)
{
    if (auto status = servicePurge(); !status) return processProgress(status);
    return FFmpegNodeRuntime::process(context);
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
    if (m_generationEnded) return processWaiting();
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
    auto generationEnd = mediaEndGeneration(buffer);
    if (!generationEnd) return processProgress(::media::Status::failure(generationEnd.error()));
    if (buffer->isEof() || buffer->isFlush() || generationEnd.value()) {
        if (generationEnd.value() && !m_group) return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Hardware transfer generation end requires a source purge owner")));
        if (generationEnd.value()) {
            auto authority = m_group->reserveGenerationArbitration();
            if (!authority) return processProgress(::media::Status::failure(authority.error()));
            auto disposition = classifyMediaAvGenerationEvidence(authority.value().reacquisition(),
                authority.value().epoch(), *generationEnd.value());
            if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
            if (disposition.value() == MediaAvGenerationEvidenceDisposition::Retired)
                return processProgress();
        }
        if (m_lineage) {
            if (auto status = m_lineage->authorizeRetainedControl(buffer); !status) return processProgress(status);
        }
        m_generationEnded = generationEnd.value().has_value();
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

    if (m_lineage) {
        const auto frameGeneration = generation(buffer);
        if (!frameGeneration) return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Hardware transfer requires source frame lineage")));
        auto disposition = m_lineage->classifyObservation(*frameGeneration);
        if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
        if (disposition.value() == MediaVideoLineageGenerationDisposition::DropStale) {
            m_pendingInput.reset();
            return processProgress();
        }
        if (auto status = m_lineage->observe(*frameGeneration); !status) return processProgress(status);
        const auto preparation = m_preparation->snapshot();
        if (preparation.generation != *frameGeneration || !preparation.releaseIdentity ||
            (m_releaseIdentity && m_releaseIdentity != preparation.releaseIdentity))
            return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Hardware transfer frame differs from its immutable preparation feed")));
        m_generation = *frameGeneration;
        m_releaseIdentity = preparation.releaseIdentity;
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

    auto output = FFmpegBufferFactory::wrapFrame(std::move(softwareFrame), MediaStreamKind::Video, MediaRuntimeLineageAllocation{});
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
            transferred, std::move(lineage), std::nullopt);
        if (!canonical) return ::media::Status::failure(canonical.error());
        transferred = std::move(canonical).value();
    } else if (const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get())) {
        if (!sourceReferenceResource()) return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("Display transfer has no source allocation owner"));
        auto display = MediaVideoDecodeBuffer::withDisplayLineage(
            *sourceReferenceResource(), transferred, source->displayLineage());
        if (!display) return ::media::Status::failure(display.error());
        transferred = std::move(display).value();
    }
    return emitTracedOutput(context, transferred);
}

} // namespace media::ffmpeg::graph
