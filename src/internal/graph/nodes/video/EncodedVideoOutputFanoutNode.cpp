#include "internal/graph/nodes/video/EncodedVideoOutputFanoutNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaBufferRef> retainPacketHeader(
    const MediaBufferRef& source, MediaBranchPublicationBoundary boundary)
{
    using Result = ::media::Result<MediaBufferRef>;
    if (source->isEof() || source->isFlush()) return Result::success(source);
    const auto* packet = FFmpegPacketView::packet(source);
    auto lineage = FFmpegPacketView::canonicalLineage(source);
    const auto& credit = FFmpegPacketView::payloadCredit(source);
    if (!packet || !packet->buf || !credit) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Encoded fanout requires reference-counted video payload and credit"));
    }
    auto cloned = FFmpegBufferFactory::clonePacket(packet, MediaStreamKind::Video);
    if (!cloned) return Result::failure(cloned.error());
    auto output = std::move(cloned).value();
    auto attached = output->attachPayloadCredit(credit);
    if (!attached) return Result::failure(attached.error());
    output->setTimeDescriptor(source->timeDescriptor());
    output->setHardwareDescriptor(source->hardwareDescriptor());
    output->setFlags(source->flags());
    if (boundary == MediaBranchPublicationBoundary::RandomAccessUnit) {
        FFmpegPacketView::writablePacket(output)->flags |= AV_PKT_FLAG_KEY;
        output->setFlags(output->flags() | MediaBufferFlag::KeyFrame);
    }
    if (!lineage) return Result::success(std::move(output));
    return MediaCanonicalAccessUnitBuffer::create(
        std::move(output), std::move(lineage), std::nullopt);
}
} // namespace

EncodedVideoOutputFanoutNode::EncodedVideoOutputFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, MediaNodeKind::EncodedVideoOutputFanout,
                        "EncodedVideoOutputFanoutNode")
{
}

::media::Status EncodedVideoOutputFanoutNode::bindJoinPlan(MediaVideoJoinPlan plan)
{
    if (m_joinPlan || m_started ||
        (plan.codec != MediaAnnexBCodec::H264 && plan.codec != MediaAnnexBCodec::Hevc))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "encoded fanout requires one prestart planned H264/HEVC join contract"));
    m_joinPlan = std::move(plan);
    return ::media::Status::success();
}

::media::Status EncodedVideoOutputFanoutNode::start(MediaGraphExecutionContext& context)
{
    if (!m_joinPlan) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "encoded fanout requires its planned packet layout and IDR contract"));
    m_started = true;
    return FFmpegNodeRuntime::start(context);
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
                     !packet->buf ||
                     !FFmpegPacketView::payloadCredit(source))) {
        return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Encoded video fanout received an incomplete immutable access unit contract")));
    }
    bool independentIdr = false;
    if (!control) {
        if (!packet->data || packet->size <= 0)
            return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "encoded fanout received an empty access unit")));
        struct AccessUnitEvidence { MediaAnnexBCodec codec; bool idr; bool otherVcl; } evidence{m_joinPlan->codec, false, false};
        auto scanned = MediaVideoNalUnitScanner::visit(
            std::span<const std::uint8_t>(packet->data, static_cast<std::size_t>(packet->size)),
            m_joinPlan->codec, m_joinPlan->packetLayout, &evidence,
            [](void* state, std::span<const std::uint8_t> unit) {
                auto& value = *static_cast<AccessUnitEvidence*>(state);
                const auto type = value.codec == MediaAnnexBCodec::H264
                    ? unit[0] & 0x1fU : (unit[0] >> 1U) & 0x3fU;
                const bool idr = value.codec == MediaAnnexBCodec::H264
                    ? type == 5U : type == 19U || type == 20U;
                const bool vcl = value.codec == MediaAnnexBCodec::H264
                    ? (type >= 1U && type <= 5U) || type == 19U || type == 20U || type == 21U
                    : type <= 31U;
                value.idr = value.idr || idr;
                value.otherVcl = value.otherVcl || (vcl && !idr);
            });
        if (!scanned) return processProgress(std::move(scanned));
        independentIdr = evidence.idr && !evidence.otherVcl;
    }
    const auto boundary = control ? MediaBranchPublicationBoundary::Control
        : independentIdr ? MediaBranchPublicationBoundary::RandomAccessUnit
                         : MediaBranchPublicationBoundary::Ordinary;
    auto published = m_fanout.publish(source, boundary, retainPacketHeader);
    if (!published) return processProgress(std::move(published));
    return source->isEof() ? processFinished() : processProgress();
}
} // namespace media::ffmpeg::graph
