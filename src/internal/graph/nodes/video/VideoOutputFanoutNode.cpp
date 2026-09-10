#include "internal/graph/nodes/video/VideoOutputFanoutNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

#include <algorithm>
#include <charconv>

namespace media::ffmpeg::graph {

VideoOutputFanoutNode::VideoOutputFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, MediaNodeKind::VideoOutputFanout,
                        "VideoOutputFanoutNode")
{
}

::media::Status VideoOutputFanoutNode::start(MediaGraphExecutionContext& context)
{
    if (nodeOption(context, "fanout.epoch.identity_transition") != "replan_session") {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "video distributor requires the planned source identity transition policy"));
    }
    const auto authority = nodeOption(context, "fanout.epoch.authority");
    MediaVideoSourceEpochPlan epoch{MediaVideoSourceEpochAuthority::CanonicalLineage, std::nullopt,
        MediaVideoSourceIdentityTransition::ReplanSession};
    if (authority == "protocol_session") {
        const auto value = nodeOption(context, "fanout.epoch.protocol_session");
        std::uint64_t parsedEpoch = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), parsedEpoch);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "video distributor requires its planned protocol session epoch"));
        }
        epoch = {MediaVideoSourceEpochAuthority::ProtocolSession, parsedEpoch,
            MediaVideoSourceIdentityTransition::ReplanSession};
    } else if (authority != "canonical_lineage") {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "video distributor requires an explicit playback epoch authority"));
    }
    {
        std::lock_guard lock(m_factsMutex);
        m_epochPlan = epoch;
        m_generation.reset();
        m_hardwareFrames.reset();
        m_frameFactsReady = false;
        m_hasObservedFrame = false;
        m_sourceContractInvalidated = false;
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Status VideoOutputFanoutNode::subscribe(
    std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge)
{
    return m_fanout.subscribe(std::move(branch), edge, MediaBranchStartGate::Immediate);
}

void VideoOutputFanoutNode::unsubscribe(std::uint64_t outputId)
{
    m_fanout.unsubscribe(outputId);
}

::media::Result<::media::ffmpeg::BufferRefPtr>
VideoOutputFanoutNode::hardwareFrames() const
{
    using Result = ::media::Result<::media::ffmpeg::BufferRefPtr>;
    std::lock_guard lock(m_factsMutex);
    if (!m_frameFactsReady || m_sourceContractInvalidated) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Shared decoder has not published frame ownership facts"));
    }
    if (!m_hardwareFrames) return Result::success({});
    auto reference = ::media::ffmpeg::BufferRefPtr(
        av_buffer_ref(m_hardwareFrames.get()));
    if (!reference) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Could not retain shared decoder hardware frame facts"));
    }
    return Result::success(std::move(reference));
}

::media::Result<std::uint64_t> VideoOutputFanoutNode::sourceGeneration() const
{
    std::lock_guard lock(m_factsMutex);
    if (!m_frameFactsReady || m_sourceContractInvalidated || !m_generation.has_value()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "Shared video source has not published playback epoch facts"));
    }
    return ::media::Result<std::uint64_t>::success(*m_generation);
}

::media::Result<MediaNodeProcessResult> VideoOutputFanoutNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (nodeOption(context, "fanout.zero_outputs") != "consume" ||
        nodeOption(context, "fanout.overflow") != "fail_branch") {
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Video output fanout requires planner lifecycle policies")));
    }
    auto input = tryPopInputOptional(context, "frame");
    if (!input) {
        return processProgress(::media::Status::failure(input.error()));
    }
    if (!input.value()) {
        auto* channel = context.findInputChannel(nodeId(), "frame");
        return channel && channel->closed() ? processFinished()
            : ::media::Result<MediaNodeProcessResult>::success(
                  MediaNodeProcessResult::waiting());
    }
    const auto& source = *input.value();
    const auto* frame = FFmpegFrameView::frame(source);
    if (!frame && !source->isEof() && !source->isFlush()) {
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video output fanout received a non-video payload")));
    }
    {
        std::lock_guard lock(m_factsMutex);
        if (!m_epochPlan) return processProgress(::media::Status::failure(
            ::media::ErrorInfo::notInitialized("video distributor playback epoch is not bound")));
        const auto* control = dynamic_cast<const MediaControlBuffer*>(source.get());
        if (m_hasObservedFrame && control && control->generation().has_value() &&
            m_generation.has_value() && *control->generation() != *m_generation) {
            m_sourceContractInvalidated = true;
            m_frameFactsReady = false;
            m_generation.reset();
            return processProgress(::media::Status::failure(::media::ErrorInfo::unsupported(
                "shared video source playback epoch changed; session replanning is required")));
        }
        // Flush is also used for same-epoch codec recovery. Revoke readiness
        // until a real frame confirms the existing contract; do not invent a
        // new epoch from an unversioned initialization/recovery control.
        if (source->isFlush()) m_frameFactsReady = false;
        if (frame && m_hasObservedFrame &&
            (frame->width != m_frameWidth || frame->height != m_frameHeight ||
             frame->format != m_frameFormat ||
             static_cast<bool>(frame->hw_frames_ctx) != static_cast<bool>(m_hardwareFrames) ||
             (frame->hw_frames_ctx && frame->hw_frames_ctx->data != m_hardwareFrames->data))) {
            m_sourceContractInvalidated = true;
            m_frameFactsReady = false;
            m_generation.reset();
            return processProgress(::media::Status::failure(::media::ErrorInfo::unsupported(
                "shared decoder frame allocation contract changed; session replanning is required")));
        }
        if (frame) {
            const auto lineage = FFmpegFrameView::canonicalLineage(source);
            if (m_epochPlan->authority == MediaVideoSourceEpochAuthority::ProtocolSession) {
                if (lineage && lineage->generation != *m_epochPlan->protocolSessionEpoch) {
                    m_sourceContractInvalidated = true;
                    m_frameFactsReady = false;
                    return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "video frame lineage conflicts with planned protocol session epoch")));
                }
                m_generation = m_epochPlan->protocolSessionEpoch;
            } else {
                if (!lineage) return processProgress(::media::Status::failure(
                    ::media::ErrorInfo::notInitialized("video source requires canonical playback lineage")));
                if (m_hasObservedFrame && m_generation.has_value() &&
                    *m_generation != lineage->generation) {
                    m_sourceContractInvalidated = true;
                    m_frameFactsReady = false;
                    return processProgress(::media::Status::failure(::media::ErrorInfo::unsupported(
                        "canonical video playback epoch changed; shared source replanning is required")));
                }
                m_generation = lineage->generation;
            }
        }
        if (frame && frame->hw_frames_ctx && !m_hardwareFrames) {
            auto reference = ::media::ffmpeg::BufferRefPtr(
                av_buffer_ref(frame->hw_frames_ctx));
            if (!reference) {
                return processProgress(::media::Status::failure(
                    ::media::ErrorInfo::allocationFailed(
                        "Could not retain decoder frame context")));
            }
            m_hardwareFrames = std::move(reference);
        }
        if (frame) {
            m_frameWidth = frame->width;
            m_frameHeight = frame->height;
            m_frameFormat = frame->format;
            m_frameFactsReady = true;
            m_hasObservedFrame = true;
        }
        if (source->isEof()) m_frameFactsReady = false;
    }
    const auto boundary = source->isEof() || source->isFlush()
        ? MediaBranchPublicationBoundary::Control : MediaBranchPublicationBoundary::Ordinary;
    auto published = m_fanout.publish(source, boundary,
        [](const MediaBufferRef& input, MediaBranchPublicationBoundary) -> ::media::Result<MediaBufferRef> {
            if (input->isEof() || input->isFlush())
                return ::media::Result<MediaBufferRef>::success(input);
            auto cloned = FFmpegBufferFactory::cloneFrame(input, MediaStreamKind::Video);
            if (!cloned) return cloned;
            auto output = std::move(cloned).value();
            output->setTimeDescriptor(input->timeDescriptor());
            output->setHardwareDescriptor(input->hardwareDescriptor());
            output->setFlags(input->flags());
            if (auto lineage = FFmpegFrameView::canonicalLineage(input))
                return MediaCanonicalVideoFrameBuffer::create(std::move(output), std::move(lineage));
            return ::media::Result<MediaBufferRef>::success(std::move(output));
        });
    if (!published) return processProgress(std::move(published));
    return source->isEof() ? processFinished() : processProgress();
}

} // namespace media::ffmpeg::graph
