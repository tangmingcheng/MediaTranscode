#include "internal/graph/nodes/output/MediaDatagramShaperNode.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramShaperNode::MediaDatagramShaperNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaMasterClock> clock) noexcept
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaDatagramShaperNode"),
      m_clock(std::move(clock))
{
}

::media::Result<std::unique_ptr<MediaDatagramShaperNode>>
MediaDatagramShaperNode::create(
    MediaNodeId nodeId,
    std::shared_ptr<MediaMasterClock> clock)
{
    using Result = ::media::Result<std::unique_ptr<MediaDatagramShaperNode>>;
    if (!nodeId.isValid() || !clock) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram shaper node requires an id and authoritative clock"));
    }
    auto node = std::unique_ptr<MediaDatagramShaperNode>(
        new (std::nothrow) MediaDatagramShaperNode(
            nodeId, std::move(clock)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramShaperNode"));
    }
    return Result::success(std::move(node));
}

MediaNodeKind MediaDatagramShaperNode::staticKind() noexcept
{
    return MediaNodeKind::DatagramShaper;
}

::media::Status MediaDatagramShaperNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto* plan = context.findInputChannel(nodeId(), "plan");
    const auto* batch = context.findInputChannel(nodeId(), "batch");
    const auto* scheduled = context.findOutputChannel(nodeId(), "scheduled");
    if (context.inputChannels(nodeId()).size() != 2 ||
        context.outputChannels(nodeId()).size() != 1 || !plan || !batch ||
        !scheduled || plan->binding().streamKind != MediaStreamKind::Metadata ||
        plan->binding().payloadKind != MediaPayloadKind::DatagramTransportPlan ||
        batch->binding().streamKind != MediaStreamKind::Metadata ||
        batch->binding().payloadKind != MediaPayloadKind::WireDatagramBatch ||
        scheduled->binding().streamKind != MediaStreamKind::Metadata ||
        scheduled->binding().payloadKind !=
            MediaPayloadKind::ScheduledWireDatagramBatch) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram shaper requires exact plan, wire batch, and scheduled ports"));
    }
    return ::media::Status::success();
}

::media::Status MediaDatagramShaperNode::start(
    MediaGraphExecutionContext& context)
{
    m_shaper.reset();
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Result<MediaNodeProcessResult>
MediaDatagramShaperNode::onProcess(MediaGraphExecutionContext& context)
{
    auto planInput = tryPopInputOptional(context, "plan");
    if (!planInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            planInput.error());
    }
    if (planInput.value()) {
        const auto* planBuffer =
            dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
                planInput.value()->get());
        if (!planBuffer) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "datagram shaper plan input requires an activated transport plan"));
        }
        auto cloned = planBuffer->plan().shaping.clone();
        if (!cloned) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                cloned.error());
        }
        if (!m_shaper) {
            auto created = MediaDatagramServiceShaper::create(
                std::move(cloned).value());
            if (!created) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    created.error());
            }
            m_shaper = std::move(created).value();
        } else {
            auto rebound = m_shaper->rebind(std::move(cloned).value());
            if (!rebound) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    rebound.error());
            }
        }
        return processProgress();
    }
    if (!m_shaper) return processWaiting();

    auto batchInput = tryPopInputOptional(context, "batch");
    if (!batchInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            batchInput.error());
    }
    if (!batchInput.value()) return processWaiting();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            batchInput.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Abort) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::cancelled(
                    "datagram shaper received abort"));
        }
        auto forwarded = emitOutput(context, "scheduled", *batchInput.value());
        return control->controlKind() == MediaControlBufferKind::Eof
            ? processFinished(std::move(forwarded))
            : processProgress(std::move(forwarded));
    }
    auto* batch = dynamic_cast<MediaWireDatagramBatchBuffer*>(
        batchInput.value()->get());
    auto now = m_clock ? m_clock->now()
                       : ::media::Result<MediaRunningTime>::failure(
                             ::media::ErrorInfo::notInitialized(
                                 "datagram shaper clock is unavailable"));
    if (!batch || !now) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            batch ? now.error()
                  : ::media::ErrorInfo::invalidArgument(
                        "datagram shaper batch input requires final wire bytes"));
    }
    auto scheduled = m_shaper->shape(*batch, now.value());
    if (!scheduled) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            scheduled.error());
    }
    return processProgress(
        emitOutput(context, "scheduled", scheduled.value()));
}

::media::Status MediaDatagramShaperNode::stop(
    MediaGraphExecutionContext& context)
{
    m_shaper.reset();
    return FFmpegNodeRuntime::stop(context);
}

void MediaDatagramShaperNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_shaper.reset();
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
