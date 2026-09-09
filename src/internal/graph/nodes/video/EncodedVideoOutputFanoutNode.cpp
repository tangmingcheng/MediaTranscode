#include "internal/graph/nodes/video/EncodedVideoOutputFanoutNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaBufferRef> retainPacketHeader(const MediaBufferRef& source)
{
    using Result = ::media::Result<MediaBufferRef>;
    if (source->isEof() || source->isFlush()) return Result::success(source);
    const auto* packet = FFmpegPacketView::packet(source);
    auto lineage = FFmpegPacketView::canonicalLineage(source);
    const auto& credit = FFmpegPacketView::payloadCredit(source);
    if (!packet || !packet->buf || !lineage || !credit) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Encoded fanout requires reference-counted video payload, credit and canonical lineage"));
    }
    auto cloned = FFmpegBufferFactory::clonePacket(packet, MediaStreamKind::Video);
    if (!cloned) return Result::failure(cloned.error());
    auto output = std::move(cloned).value();
    auto attached = output->attachPayloadCredit(credit);
    if (!attached) return Result::failure(attached.error());
    output->setTimeDescriptor(source->timeDescriptor());
    output->setHardwareDescriptor(source->hardwareDescriptor());
    output->setFlags(source->flags());
    return MediaCanonicalAccessUnitBuffer::create(
        std::move(output), std::move(lineage), std::nullopt);
}
} // namespace

EncodedVideoOutputFanoutNode::EncodedVideoOutputFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, MediaNodeKind::EncodedVideoOutputFanout,
                        "EncodedVideoOutputFanoutNode")
{
}

::media::Status EncodedVideoOutputFanoutNode::subscribe(
    std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge)
{
    return m_fanout.subscribe(std::move(branch), edge, MediaBranchStartGate::RandomAccessUnit);
}

void EncodedVideoOutputFanoutNode::unsubscribe(std::uint64_t outputId)
{
    m_fanout.unsubscribe(outputId);
}

::media::Result<MediaNodeProcessResult> EncodedVideoOutputFanoutNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (nodeOption(context, "fanout.zero_outputs") != "consume" ||
        nodeOption(context, "fanout.overflow") != "fail_branch") {
        return processProgress(::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Encoded video fanout requires planner lifecycle policies")));
    }
    auto input = tryPopInputOptional(context, "packet");
    if (!input) return processProgress(::media::Status::failure(input.error()));
    if (!input.value()) {
        const auto* channel = context.findInputChannel(nodeId(), "packet");
        return channel && channel->closed() ? processFinished()
            : ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }
    const auto& source = *input.value();
    const auto* packet = FFmpegPacketView::packet(source);
    const bool control = source->isEof() || source->isFlush();
    if (!control && (!packet || source->streamKind() != MediaStreamKind::Video ||
                     !packet->buf || !FFmpegPacketView::canonicalLineage(source) ||
                     !FFmpegPacketView::payloadCredit(source))) {
        return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Encoded video fanout received an incomplete immutable access unit contract")));
    }
    const auto boundary = control ? MediaBranchPublicationBoundary::Control
        : (packet->flags & AV_PKT_FLAG_KEY) ? MediaBranchPublicationBoundary::RandomAccessUnit
                                          : MediaBranchPublicationBoundary::Ordinary;
    auto published = m_fanout.publish(source, boundary, retainPacketHeader);
    if (!published) return processProgress(std::move(published));
    return source->isEof() ? processFinished() : processProgress();
}
} // namespace media::ffmpeg::graph
